
#ifndef __MESHOPS_H
#define __MESHOPS_H

#include "mesh.h"
#include "../mtwist/mtwist.h"

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
	__forceinline void* operator new[]   (size_t x){ return _aligned_malloc(x, 16); }
	__forceinline void* operator new     (size_t x){ return _aligned_malloc(x, 16); }
	__forceinline void  operator delete[](void*  x) { if (x) _aligned_free(x); }
	__forceinline void  operator delete  (void*  x) { if (x) _aligned_free(x); }
};

class MeshySet : public Meshy {
public:

	MeshySet(const Mesh * const mesh, mat4& xform) :Meshy(mesh), xform(xform) { }
	virtual void begin(const int t){
		int& idx = this->idx[t];
		idx = 0;
	}
	virtual bool next(const int t, mat4& m) {
		int& idx = this->idx[t];
		if (idx == 0) {
			m = xform;
			idx++;
			return true;
		} else {
			return false;
		}
	}
	virtual void fbegin(const int t) {
		int& fidx = this->fidx[t];
		int& idx = this->idx[t];
		fidx = 0;
	}
	virtual bool fnext(const int t, Face& f) {
		int& fidx = this->fidx[t];
		int& idx = this->idx[t];
		if (fidx < mesh->faces.size()) {
			f = mesh->faces[fidx];
			fidx++;
			return true;
		} else {
			return false;
		}
	}
private:
	std::array<int,16> idx;
	std::array<int, 16> fidx;
	mat4 xform;
};


class MeshyTranslate : public Meshy {
public:
	MeshyTranslate(Meshy& inmesh, mat4& xform) :Meshy(inmesh.mesh), in(inmesh), xform(xform) { }

	virtual void begin(const int t) {
		int& idx = this->idx[t];
		idx = 0;
		in.begin(t);
	}
	virtual bool next(const int t, mat4& m) {
		int& idx = this->idx[t];

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
		int& idx = this->idx[t];
		in.fbegin(t);
	}
	virtual bool fnext(const int t, Face& f) {
		int& idx = this->idx[t];
		return in.fnext(t, f);
	}

private:
	Meshy& in;
	mat4 xform;
	std::array<int,16> idx;
	std::array<int, 16> fidx;
};

class MeshyMultiply : public Meshy {
public:
	MeshyMultiply(Meshy& inmesh, int many, vec3& translate, vec3& scale) :Meshy(inmesh.mesh), in(inmesh), many(many), translate(translate), scale(scale) {}

	virtual void begin(const int t) {
		int& idx = this->idx[t];
		mat4& xform = this->xform[t];

		idx = 0; 
		calc(idx, xform);
		in.begin(t);
	}
	virtual bool next(const int t, mat4& m) {
		int& idx = this->idx[t];
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
		int& idx = this->idx[t];
		in.fbegin(t);
	}
	virtual bool fnext(const int t, Face& f) {
		int& idx = this->idx[t];
		return in.fnext(t, f);
	}

	__forceinline void calc(const int idx, mat4& xform) {
		mat4 tr = mat4::position(translate*vec3(float(idx)));
		mat4 sc = mat4::scale(vec3(1.0f) + scale*vec3(float(idx)));
		mat4_mul(tr, sc, xform);
	}
private:
	Meshy& in;
	int many;
	vec3 translate;
	vec3 scale;

	std::array<mat4,16> xform;
	std::array<int,16> idx;
	std::array<int, 16> fidx;
};

class MeshyCenter : public Meshy {
public:
	MeshyCenter(Meshy& inmesh, const bool center_x, const bool center_y, const bool center_z, const bool y_on_floor) :Meshy(inmesh.mesh), in(inmesh), center_x(center_x), center_y(center_y), center_z(center_z), y_on_floor(y_on_floor){}

	virtual void begin(const int t) {
		int& idx = this->idx[t];
		mat4& xform = this->xform[t];

		vec4 bbox_min;
		vec4 bbox_max;
		in.begin(t);
		mat4 in_mat;
		in.next(t, in_mat);
		vec4 xb = mat4_mul(in_mat, mesh->bbox[0]);
		bbox_min = bbox_max = xb;

		in.begin(t);
		while ( in.next(t, in_mat) ) {
			for (unsigned i=0; i<8; i++ ){
				vec4 xb = mat4_mul(in_mat, mesh->bbox[i]);
				bbox_min = vmin(bbox_min, xb);
				bbox_max = vmax(bbox_max, xb);
			}
		}

		float& minx = bbox_min.x;
		float& miny = bbox_min.y;
		float& minz = bbox_min.z;
		float& maxx = bbox_max.x;
		float& maxy = bbox_max.y;
		float& maxz = bbox_max.z;

		/*
		vec4 centered = -(bbox_min + (bbox_max - bbox_min) / vec4(2.0f));
		ivec4 mask(center_x ? -1 : 0, center_y ? -1 : 0, center_z ? -1 : 0, 0);
		*/

		// calculate mat4 based on mins and maxs
		const float move_x = center_x ? -(minx + (maxx - minx) / 2) : 0;
		float move_y;
		if (!y_on_floor) {
			move_y = center_y ? -(miny + (maxy - miny) / 2) : 0;
		} else {
			move_y = center_y ? 0 - miny : 0;
		}
		const float move_z = center_z ? -(minz + (maxz - minz) / 2) : 0;
//		cout << "move:" << vec3(move_x, move_y, move_z) << endl;

		xform = mat4::position({ move_x, move_y, move_z, 1 });

		idx = 0;
		in.begin(t);
	}

	virtual bool next(const int t, mat4& m) {
		int& idx = this->idx[t];
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
		int& idx = this->idx[t];
		in.fbegin(t);
	}
	virtual bool fnext(const int t, Face& f) {
		int& idx = this->idx[t];
		return in.fnext(t, f);
	}
private:
	Meshy& in;
	const bool center_x;
	const bool center_y;
	const bool center_z;
	const bool y_on_floor;

	std::array<mat4,16> xform;
	std::array<int,16> idx;
	std::array<int, 16> fidx;
};


class MeshyScatter : public Meshy {
public:
	MeshyScatter(Meshy& inmesh, int many, vec3& position) :Meshy(inmesh.mesh), in(inmesh), many(many), position(position) {}

	virtual void begin(const int t) {
		int& idx = this->idx[t];
		mt_prng& mt = this->mt[t];

		idx = 0;
		mt.seed32(345894);
		in.begin(t);
	}
	virtual bool next(const int t, mat4& m) {
		int& idx = this->idx[t];
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

		auto this_pos = vec3(mt(), mt(), mt()) * position;
		mat4_mul(mat4::position(this_pos), in_mat, m);
		return true;
	}
	virtual void fbegin(const int t) {
		int& idx = this->idx[t];
		in.fbegin(t);
	}
	virtual bool fnext(const int t, Face& f) {
		int& idx = this->idx[t];
		return in.fnext(t, f);
	}

private:
	Meshy& in;
	int many;
	vec3 position;

	std::array<int,16> idx;
	std::array<int, 16> fidx;
	std::array<mt_prng, 16> mt;
};

#endif //__MESHOPS_H
