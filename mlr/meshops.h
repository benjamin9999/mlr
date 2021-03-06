
#ifndef __MESHOPS_H
#define __MESHOPS_H

#include "mesh.h"
#include "../mtwist/mtwist.h"
#include "framestack.h"

//local rotate x,y,z
//scale x,y,z
//rotate x,y,z
//translate x,y,z
//extrude-normal float
//count int, min=1
class Meshy {
public:
	const Mesh * mesh;
	Meshy(const Mesh * const mesh) : mesh(mesh) {}
	virtual void begin(const int t) = 0;
	virtual bool next(const int t, mat4& m) = 0;
	virtual void fbegin(const int t) = 0;
	virtual bool fnext(const int t, struct Face& face) = 0;
	virtual bool shadows_enabled() {
		return false;
	}
	__forceinline void* operator new[]   (size_t x){ return fs_alloc(x); }
	__forceinline void* operator new     (size_t x){ return fs_alloc(x); }
	__forceinline void  operator delete[](void*  x) {}
	__forceinline void  operator delete  (void*  x) {}
};


class MeshySet : public Meshy {
public:

	MeshySet(const Mesh * const mesh, mat4& xform) :Meshy(mesh), xform(xform) { }
	virtual void begin(const int t){
		auto& idx = this->idx[t];
		idx = 0;
	}
	virtual bool next(const int t, mat4& m) {
		auto& idx = this->idx[t];
		if (idx == 0) {
			m = xform;
			idx++;
			return true;
		} else {
			return false;
		}
	}
	virtual void fbegin(const int t) {
		auto& fidx = this->fidx[t];
		auto& idx = this->idx[t];
		fidx = 0;
	}
	virtual bool fnext(const int t, Face& f) {
		auto& fidx = this->fidx[t];
		auto& idx = this->idx[t];
		if (fidx < mesh->faces.size()) {
			f = mesh->faces[fidx];
			fidx++;
			return true;
		} else {
			return false;
		}
	}
private:
	std::array<unsigned,16> idx;
	std::array<unsigned,16> fidx;
	mat4 xform;
};


class MeshyShadows : public Meshy {
public:
	MeshyShadows(Meshy& inmesh) :Meshy(inmesh.mesh), in(inmesh) {}
	virtual void begin(const int t) {
		in.begin(t);
	}
	virtual bool next(const int t, mat4& m) {
		return in.next(t,m);
	}
	virtual void fbegin(const int t) {
		in.fbegin(t);
	}
	virtual bool fnext(const int t, Face& f) {
		return in.fnext(t, f);
	}
	virtual bool shadows_enabled() {
		return true;
	}
private:
	Meshy& in;
};


class MeshyTranslate : public Meshy {
public:
	MeshyTranslate(Meshy& inmesh, mat4& xform) :Meshy(inmesh.mesh), in(inmesh), xform(xform) { }

	virtual void begin(const int t) {
		auto& idx = this->idx[t];
		idx = 0;
		in.begin(t);
	}
	virtual bool next(const int t, mat4& m) {
		auto& idx = this->idx[t];

		mat4 in_mat;
		auto alive = in.next(t, in_mat);
		if (alive) {
			mat4_mul(xform, in_mat, m);
			idx++;
			return true;
		} else {
			return false;
		}
	}
	virtual void fbegin(const int t) {
		auto& idx = this->idx[t];
		in.fbegin(t);
	}
	virtual bool fnext(const int t, Face& f) {
		auto& idx = this->idx[t];
		return in.fnext(t, f);
	}

private:
	Meshy& in;
	mat4 xform;
	std::array<unsigned,16> idx;
	std::array<unsigned,16> fidx;
};

class MeshyMultiply : public Meshy {
public:
	MeshyMultiply(Meshy& inmesh, int many, vec3& translate, vec3& scale) :Meshy(inmesh.mesh), in(inmesh), many(many), translate(translate.x,translate.y,translate.z,1), scale(scale.x,scale.y,scale.z,1) {}

	virtual void begin(const int t) {
		auto& idx = this->idx[t];
		mat4& xform = this->xform[t];

		idx = 0; 
		calc(idx, xform);
		in.begin(t);
	}
	virtual bool next(const int t, mat4& m) {
		auto& idx = this->idx[t];
		mat4& xform = this->xform[t];

		if (idx == many) return false;

		mat4 in_mat;
		auto alive = in.next(t, in_mat);
		if (!alive) {
			idx++;
			calc(idx, xform);
			if (idx == many) return false;
			in.begin(t);
			in.next(t, in_mat);
		}

		mat4_mul(xform, in_mat, m);
		return true;
	}
	virtual void fbegin(const int t) {
		auto& idx = this->idx[t];
		in.fbegin(t);
	}
	virtual bool fnext(const int t, Face& f) {
		auto& idx = this->idx[t];
		return in.fnext(t, f);
	}

	__forceinline void calc(const int idx, mat4& xform) {
		auto fidx = vec4{ float(idx), float(idx), float(idx), 1 };
		mat4 tr = mat4::position(translate*fidx);
		mat4 sc = mat4::scale(vec4(1,1,1,1) + scale*fidx);
		mat4_mul(tr, sc, xform);
	}
private:
	Meshy& in;
	int many;
	vec4 translate;
	vec4 scale;

	std::array<mat4,16> xform;
	std::array<unsigned,16> idx;
	std::array<unsigned,16> fidx;
};

class MeshyCenter : public Meshy {
public:
	MeshyCenter(Meshy& inmesh, const bool center_x, const bool center_y, const bool center_z, const bool y_on_floor) :Meshy(inmesh.mesh), in(inmesh), center_x(center_x), center_y(center_y), center_z(center_z), y_on_floor(y_on_floor){}

	virtual void begin(const int t) {
		auto& idx = this->idx[t];
		mat4& xform = this->xform[t];

		auto bbox_min = vec4(1e20F);
		auto bbox_max = vec4(-1e20F);
		mat4 in_mat;

		in.begin(t);
		while (in.next(t, in_mat)) {
			for (unsigned i=0; i<8; i++ ){
				vec4 xb = mat4_mul(in_mat, mesh->bbox[i]);
				bbox_min = vmin(bbox_min, xb);
				bbox_max = vmax(bbox_max, xb);
			}
		}

		auto center_adjust = -(bbox_min + (bbox_max - bbox_min)*vec4(0.5f));

		const float move_x = center_x ? center_adjust._x() : 0;
		float move_y;
		if (!y_on_floor) {
			move_y = center_y ? center_adjust._y() : 0;
		} else {
			move_y = center_y ? 0 - bbox_min._y() : 0;
		}
		const float move_z = center_z ? center_adjust._z() : 0;

		xform = mat4::position({ move_x, move_y, move_z, 1 });

		idx = 0;
		in.begin(t);
	}

	virtual bool next(const int t, mat4& m) {
		auto& idx = this->idx[t];
		mat4& xform = this->xform[t];

		mat4 in_mat;
		auto alive = in.next(t, in_mat);
		if (alive) {
//			m = mat4_mul(xform, in_mat);
			mat4_mul(xform, in_mat, m);
			idx++;
			return true;
		} else {
			return false;
		}
	}
	virtual void fbegin(const int t) {
		auto& idx = this->idx[t];
		in.fbegin(t);
	}
	virtual bool fnext(const int t, Face& f) {
		auto& idx = this->idx[t];
		return in.fnext(t, f);
	}
private:
	Meshy& in;
	const bool center_x;
	const bool center_y;
	const bool center_z;
	const bool y_on_floor;

	std::array<mat4,16> xform;
	std::array<unsigned,16> idx;
	std::array<unsigned,16> fidx;
};


class MeshyScatter : public Meshy {
public:
	MeshyScatter(Meshy& inmesh, int many, vec3& position) :Meshy(inmesh.mesh), in(inmesh), many(many), position({ position.x, position.y, position.z, 1 }) {}

	virtual void begin(const int t) {
		auto& idx = this->idx[t];
		mt_prng& mt = this->mt[t];

		idx = 0;
		mt.seed32(345894);
		in.begin(t);
	}
	virtual bool next(const int t, mat4& m) {
		auto& idx = this->idx[t];
		mt_prng& mt = this->mt[t];

		if (idx == many) return false;

		mat4 in_mat;
		auto alive = in.next(t, in_mat);
		if (!alive) {
			idx++;
			if (idx == many) return false;
			in.begin(t);
			in.next(t, in_mat);
		}

		auto this_pos = vec4(mt()*2-1, mt()*2-1, mt()*2-1,1) * position;
		mat4_mul(mat4::position(this_pos), in_mat, m);
		return true;
	}
	virtual void fbegin(const int t) {
		auto& idx = this->idx[t];
		in.fbegin(t);
	}
	virtual bool fnext(const int t, Face& f) {
		auto& idx = this->idx[t];
		return in.fnext(t, f);
	}

private:
	Meshy& in;
	int many;
	vec4 position;

	std::array<unsigned,16> idx;
	std::array<unsigned,16> fidx;
	std::array<mt_prng, 16> mt;
};


class MeshyMaterial : public Meshy {
public:
	MeshyMaterial(Meshy& inmesh, const int matlow, const int mathigh) :Meshy(inmesh.mesh), in(inmesh), matlow(matlow), mathigh(mathigh) {}

	virtual void begin(const int t) {
		auto& idx = this->idx[t];
		idx = 0;
		in.begin(t);
	}
	virtual bool next(const int t, mat4& m) {
		auto& idx = this->idx[t];
		mat4 in_mat;
		auto alive = in.next(t, m);
		idx++;
		return alive;
	}
	virtual void fbegin(const int t) {
		auto& idx = this->idx[t];
		auto& matidx = this->matidx[t];
		in.fbegin(t);
		matmod = mathigh - matlow;
		matidx = (idx % (matmod+1)) + matlow;
	}
	virtual bool fnext(const int t, Face& f) {
		auto& idx = this->idx[t];
		auto& matidx = this->matidx[t];
		auto alive = in.fnext(t, f);
		f.mf = matidx;
		return alive;
	}

private:
	Meshy& in;
	std::array<unsigned,16> idx;
	std::array<unsigned,16> fidx;
	std::array<unsigned,16> matidx;
	const int matlow;
	const int mathigh;
	int matmod;
};



class MeshyMultiply2 : public Meshy {
public:
	MeshyMultiply2(Meshy& inmesh, int many, vec3& translate, vec3& rotate) :Meshy(inmesh.mesh), in(inmesh), many(many), translate(translate), rotate(rotate) {}

	virtual void begin(const int t) {
		auto& idx = this->idx[t];
		mat4& xform = this->xform[t];

		idx = 0;
		xform = mat4::ident();
//		calc(idx, xform);
		in.begin(t);
	}
	virtual bool next(const int t, mat4& m) {
		auto& idx = this->idx[t];
		mat4& xform = this->xform[t];

		if (idx == many) return false;

		mat4 in_mat;
		auto alive = in.next(t, in_mat);
		if (!alive) {
			idx++;
			calc(idx, xform);
			if (idx == many) return false;
			in.begin(t);
			in.next(t, in_mat);
		}

		mat4_mul(xform, in_mat, m);
		return true;
	}
	virtual void fbegin(const int t) {
		auto& idx = this->idx[t];
		in.fbegin(t);
	}
	virtual bool fnext(const int t, Face& f) {
		auto& idx = this->idx[t];
		return in.fnext(t, f);
	}

	__forceinline void calc(const int idx, mat4& xform) {
		mat4 tr = mat4::position(translate);
		mat4 rx = mat4::rotate_x(rotate.x);
		mat4 ry = mat4::rotate_y(rotate.y);
		mat4 rz = mat4::rotate_z(rotate.z);
		xform = mat4_mul(xform, mat4_mul(tr, mat4_mul(rx, mat4_mul(ry, rz))));

//		mat4 sc = mat4::scale(vec3(1.0f) + scale*vec3(float(idx)));
//		mat4_mul(tr, sc, xform);
	}
private:
	Meshy& in;
	int many;
	vec3 translate;
	vec3 rotate;

	std::array<mat4,16> xform;
	std::array<unsigned,16> idx;
	std::array<unsigned,16> fidx;
};

#endif //__MESHOPS_H

