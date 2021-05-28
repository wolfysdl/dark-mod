/*****************************************************************************
The Dark Mod GPL Source Code

This file is part of the The Dark Mod Source Code, originally based
on the Doom 3 GPL Source Code as published in 2011.

The Dark Mod Source Code is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version. For details, see LICENSE.TXT.

Project: The Dark Mod (http://www.thedarkmod.com/)

******************************************************************************/

#include "precompiled.h"
#pragma hdrstop


//===============================================================
//
//	idWinding
//
//===============================================================

/*
=============
idWinding::ReAllocate
=============
*/
bool idWinding::ReAllocate( int n, bool keep ) {
	idVec5 *oldP;

	oldP = p;
	n = (n+3) & ~3;	// align up to multiple of four
	p = new idVec5[n];
	if ( oldP ) {
		if ( keep ) {
			memcpy( p, oldP, numPoints * sizeof(p[0]) );
		}
		delete[] oldP;
	}
	allocedSize = n;

	return true;
}

/*
=============
idWinding::BaseForPlane
=============
*/
void idWinding::BaseForPlane( const idVec3 &normal, const float dist ) {
	idVec3 org, vright, vup;

	org = normal * dist;

	normal.NormalVectors( vup, vright );
	vup *= MAX_WORLD_SIZE;
	vright *= MAX_WORLD_SIZE;

	EnsureAlloced( 4 );
	numPoints = 4;
	p[0].ToVec3() = org - vright + vup;
	p[0].s = p[0].t = 0.0f;
	p[1].ToVec3() = org + vright + vup;
	p[1].s = p[1].t = 0.0f;
	p[2].ToVec3() = org + vright - vup;
	p[2].s = p[2].t = 0.0f;
	p[3].ToVec3() = org - vright - vup;
	p[3].s = p[3].t = 0.0f;
}

/*
=============
idWinding::SetTrimmedPlane
=============
*/
idWinding *idWinding::CreateTrimmedPlane( const idPlane &mainPlane, int numCuts, const idPlane *cutPlanes, float epsilon, IncidentPlaneMode incidentPlaneMode ) {
	//establish local coordinate system
	idVec3 origin, normal, axisU, axisV;
	normal = mainPlane.Normal();
	origin = mainPlane.Dist() * normal;
	normal.NormalVectors(axisV, axisU);

	idList<bool> retainOn;
	retainOn.SetNum(numCuts);
	for (int i = 0; i < numCuts; i++) {
		float dot = cutPlanes[i].Normal() * normal;
		//angle <~ 1e-2  =>  planes are incident
		if (dot >= 1.0f - 1e-4f)
			retainOn[i] = !!(incidentPlaneMode & INCIDENT_PLANE_RETAIN_CODIRECT);
		else if (dot <= -(1.0f - 1e-4f))
			retainOn[i] = !!(incidentPlaneMode & INCIDENT_PLANE_RETAIN_OPPOSITE);
		else
			retainOn[i] = false;
	}

	auto ComputeBoundingUvBox = [origin, axisU, axisV](const idWinding &w) -> idBounds {
		idBounds box;
		box.Clear();
		for (int j = 0; j < w.numPoints; j++) {
			const idVec3 &xyz = w[j].ToVec3();
			box.AddPoint(idVec3((xyz - origin) * axisU, (xyz - origin) * axisV, 0.0f));
		}
		return box;
	};

	//start with huge UV-box winding
	idWinding *w = new idWinding(mainPlane);
	w->EnsureAlloced(numCuts + 4, true);
	//clip the winding with all trimming planes
	idBounds uvbox = ComputeBoundingUvBox(*w);
	for (int i = 0; i < numCuts; i++) {
		w = w->Clip(cutPlanes[i], 0.0f, retainOn[i]);
		if (!w)
			break;
		uvbox = ComputeBoundingUvBox(*w);
	}
	//note: if result is empty, then we take last non-empty winding as estimate
	//this may be important if true winding is very small but has disappeared due to floating point errors

	//this expansion should be larger than possible round-off errors
	uvbox.ExpandSelf(1.0f);

	//regenerate initial winding from estimated UV-box
	if (!w)
		w = new idWinding(numCuts + 4);
	w->numPoints = 4;
	w->p[0].ToVec3() = origin + uvbox[0].x * axisU + uvbox[1].y * axisV;
	w->p[1].ToVec3() = origin + uvbox[1].x * axisU + uvbox[1].y * axisV;
	w->p[2].ToVec3() = origin + uvbox[1].x * axisU + uvbox[0].y * axisV;
	w->p[3].ToVec3() = origin + uvbox[0].x * axisU + uvbox[0].y * axisV;
	for (int v = 0; v < 4; v++)
		w->p[v].s = w->p[v].t = 0.0f;

	//clip the winding with all trimming planes
	//since initial UV-box is not huge this time, we expect to get much more precise vertices
	for (int i = 0; i < numCuts; i++) {
		w = w->Clip(cutPlanes[i], epsilon, retainOn[i]);
		if (!w)
			break;
	}

	return w;
}

/*
=============
idWinding::Split
=============
*/
int idWinding::Split( const idPlane &plane, const float epsilon, idWinding **front, idWinding **back ) const {
	double *			dists;
	byte *			sides;
	int				counts[3];
	float			dot;
	int				i, j;
	const idVec5 *	p1, *p2;
	idVec5			mid;
	idWinding *		f, *b;
	int				maxpts;

	assert( this );

	dists = (double *) _alloca( (numPoints+4) * sizeof( double ) );
	sides = (byte *) _alloca( (numPoints+4) * sizeof( byte ) );

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for ( i = 0; i < numPoints; i++ ) {
		dists[i] = dot = idVec3d(plane.Normal()).Dot(idVec3d(p[i].ToVec3())) + plane[3];
		if ( dot > epsilon ) {
			sides[i] = SIDE_FRONT;
		} else if ( dot < -epsilon ) {
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];
	
	*front = *back = NULL;

	// if coplanar, put on the front side if the normals match
	if ( !counts[SIDE_FRONT] && !counts[SIDE_BACK] ) {
		idPlane windingPlane;

		GetPlane( windingPlane );
		if ( windingPlane.Normal() * plane.Normal() > 0.0f ) {
			*front = Copy();
			return SIDE_FRONT;
		} else {
			*back = Copy();
			return SIDE_BACK;
		}
	}
	// if nothing at the front of the clipping plane
	if ( !counts[SIDE_FRONT] ) {
		*back = Copy();
		return SIDE_BACK;
	}
	// if nothing at the back of the clipping plane
	if ( !counts[SIDE_BACK] ) {
		*front = Copy();
		return SIDE_FRONT;
	}

	maxpts = numPoints+4;	// cant use counts[0]+2 because of fp grouping errors

	*front = f = new idWinding(maxpts);
	*back = b = new idWinding(maxpts);
		
	for (i = 0; i < numPoints; i++) {
		p1 = &p[i];
		
		if ( sides[i] == SIDE_ON ) {
			f->p[f->numPoints] = *p1;
			f->numPoints++;
			b->p[b->numPoints] = *p1;
			b->numPoints++;
			continue;
		}
	
		if ( sides[i] == SIDE_FRONT ) {
			f->p[f->numPoints] = *p1;
			f->numPoints++;
		}

		if ( sides[i] == SIDE_BACK ) {
			b->p[b->numPoints] = *p1;
			b->numPoints++;
		}

		if ( sides[i+1] == SIDE_ON || sides[i+1] == sides[i] ) {
			continue;
		}
			
		// generate a split point
		p2 = &p[(i+1)%numPoints];
		
		// always calculate the split going from the same side
		// or minor epsilon issues can happen
		if ( sides[i] == SIDE_FRONT ) {
			dot = dists[i] / ( dists[i] - dists[i+1] );
			for ( j = 0; j < 3; j++ ) {
				// avoid round off error when possible
				if ( plane.Normal()[j] == 1.0f ) {
					mid[j] = plane.Dist();
				} else if ( plane.Normal()[j] == -1.0f ) {
					mid[j] = -plane.Dist();
				} else {
					mid[j] = (*p1)[j] + dot * ( (*p2)[j] - (*p1)[j] );
				}
			}
			mid.s = p1->s + dot * ( p2->s - p1->s );
			mid.t = p1->t + dot * ( p2->t - p1->t );
		} else {
			dot = dists[i+1] / ( dists[i+1] - dists[i] );
			for ( j = 0; j < 3; j++ ) {	
				// avoid round off error when possible
				if ( plane.Normal()[j] == 1.0f ) {
					mid[j] = plane.Dist();
				} else if ( plane.Normal()[j] == -1.0f ) {
					mid[j] = -plane.Dist();
				} else {
					mid[j] = (*p2)[j] + dot * ( (*p1)[j] - (*p2)[j] );
				}
			}
			mid.s = p2->s + dot * ( p1->s - p2->s );
			mid.t = p2->t + dot * ( p1->t - p2->t );
		}

		f->p[f->numPoints] = mid;
		f->numPoints++;
		b->p[b->numPoints] = mid;
		b->numPoints++;
	}
	
	if ( f->numPoints > maxpts || b->numPoints > maxpts ) {
		idLib::common->FatalError( "idWinding::Split: points exceeded estimate." );
	}

	return SIDE_CROSS;
}

/*
=============
idWinding::Clip
=============
*/
idWinding *idWinding::Clip( const idPlane &plane, const float epsilon, const bool keepOn ) {
	double *		dists;
	byte *		sides;
	idVec5 *	newPoints;
	int			newNumPoints;
	int			counts[3];
	float		dot;
	int			i, j;
	idVec5 *	p1, *p2;
	idVec5		mid;
	int			maxpts;

	assert( this );

	dists = (double *) _alloca( (numPoints+4) * sizeof( double ) );
	sides = (byte *) _alloca( (numPoints+4) * sizeof( byte ) );

	counts[SIDE_FRONT] = counts[SIDE_BACK] = counts[SIDE_ON] = 0;

	// determine sides for each point
	for ( i = 0; i < numPoints; i++ ) {
		dists[i] = dot = idVec3d(plane.Normal()).Dot(idVec3d(p[i].ToVec3())) + plane[3];
		if ( dot > epsilon ) {
			sides[i] = SIDE_FRONT;
		} else if ( dot < -epsilon ) {
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	// if the winding is on the plane and we should keep it
	if ( keepOn && !counts[SIDE_FRONT] && !counts[SIDE_BACK] ) {
		return this;
	}
	// if nothing at the front of the clipping plane
	if ( !counts[SIDE_FRONT] ) {
		delete this;
		return NULL;
	}
	// if nothing at the back of the clipping plane
	if ( !counts[SIDE_BACK] ) {
		return this;
	}

	maxpts = numPoints + 4;		// cant use counts[0]+2 because of fp grouping errors

	newPoints = (idVec5 *) _alloca16( maxpts * sizeof( idVec5 ) );
	newNumPoints = 0;
		
	for ( i = 0; i < numPoints; i++ ) {
		p1 = &p[i];

		if ( newNumPoints+1 > maxpts ) {
			return this;		// can't split -- fall back to original
		}

		if ( sides[i] == SIDE_ON ) {
			newPoints[newNumPoints] = *p1;
			newNumPoints++;
			continue;
		}

		if ( sides[i] == SIDE_FRONT ) {
			newPoints[newNumPoints] = *p1;
			newNumPoints++;
		}

		if ( sides[i+1] == SIDE_ON || sides[i+1] == sides[i] ) {
			continue;
		}

		if ( newNumPoints+1 > maxpts ) {
			return this;		// can't split -- fall back to original
		}

		// generate a split point
		p2 = &p[(i+1)%numPoints];

		dot = dists[i] / (dists[i] - dists[i+1]);
		for ( j = 0; j < 3; j++ ) {
			// avoid round off error when possible
			if ( plane.Normal()[j] == 1.0f ) {
				mid[j] = plane.Dist();
			} else if ( plane.Normal()[j] == -1.0f ) {
				mid[j] = -plane.Dist();
			} else {
				mid[j] = (*p1)[j] + dot * ( (*p2)[j] - (*p1)[j] );
			}
		}
		mid.s = p1->s + dot * ( p2->s - p1->s );
		mid.t = p1->t + dot * ( p2->t - p1->t );

		newPoints[newNumPoints] = mid;
		newNumPoints++;
	}
	
	if ( !EnsureAlloced( newNumPoints, false ) ) {
		return this;
	}

	numPoints = newNumPoints;
	memcpy( p, newPoints, newNumPoints * sizeof(idVec5) );

	return this;
}

/*
=============
idWinding::ClipInPlace
=============
*/
bool idWinding::ClipInPlace( const idPlane &plane, const float epsilon, const bool keepOn ) {
	double*		dists;
	byte *		sides;
	idVec5 *	newPoints;
	int			newNumPoints;
	int			counts[3];
	float		dot;
	int			i, j;
	idVec5 *	p1, *p2;
	idVec5		mid;
	int			maxpts;

	assert( this );

	dists = (double *) _alloca( (numPoints+4) * sizeof( double ) );
	sides = (byte *) _alloca( (numPoints+4) * sizeof( byte ) );

	counts[SIDE_FRONT] = counts[SIDE_BACK] = counts[SIDE_ON] = 0;

	// determine sides for each point
	for ( i = 0; i < numPoints; i++ ) {
		dists[i] = dot = idVec3d(plane.Normal()).Dot(idVec3d(p[i].ToVec3())) + plane[3];
		if ( dot > epsilon ) {
			sides[i] = SIDE_FRONT;
		} else if ( dot < -epsilon ) {
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];
	
	// if the winding is on the plane and we should keep it
	if ( keepOn && !counts[SIDE_FRONT] && !counts[SIDE_BACK] ) {
		return true;
	}
	// if nothing at the front of the clipping plane
	if ( !counts[SIDE_FRONT] ) {
		numPoints = 0;
		return false;
	}
	// if nothing at the back of the clipping plane
	if ( !counts[SIDE_BACK] ) {
		return true;
	}

	maxpts = numPoints + 4;		// cant use counts[0]+2 because of fp grouping errors

	newPoints = (idVec5 *) _alloca16( maxpts * sizeof( idVec5 ) );
	newNumPoints = 0;

	for ( i = 0; i < numPoints; i++ ) {
		p1 = &p[i];

		if ( newNumPoints+1 > maxpts ) {
			return true;		// can't split -- fall back to original
		}
		
		if ( sides[i] == SIDE_ON ) {
			newPoints[newNumPoints] = *p1;
			newNumPoints++;
			continue;
		}
	
		if ( sides[i] == SIDE_FRONT ) {
			newPoints[newNumPoints] = *p1;
			newNumPoints++;
		}

		if ( sides[i+1] == SIDE_ON || sides[i+1] == sides[i] ) {
			continue;
		}
			
		if ( newNumPoints+1 > maxpts ) {
			return true;		// can't split -- fall back to original
		}

		// generate a split point
		p2 = &p[(i+1)%numPoints];
		
		dot = dists[i] / (dists[i] - dists[i+1]);
		for ( j = 0; j < 3; j++ ) {
			// avoid round off error when possible
			if ( plane.Normal()[j] == 1.0f ) {
				mid[j] = plane.Dist();
			} else if ( plane.Normal()[j] == -1.0f ) {
				mid[j] = -plane.Dist();
			} else {
				mid[j] = (*p1)[j] + dot * ( (*p2)[j] - (*p1)[j] );
			}
		}
		mid.s = p1->s + dot * ( p2->s - p1->s );
		mid.t = p1->t + dot * ( p2->t - p1->t );
			
		newPoints[newNumPoints] = mid;
		newNumPoints++;
	}

	if ( !EnsureAlloced( newNumPoints, false ) ) {
		return true;
	}

	numPoints = newNumPoints;
	memcpy( p, newPoints, newNumPoints * sizeof(idVec5) );

	return true;
}

/*
=============
idWinding::Copy
=============
*/
idWinding *idWinding::Copy( void ) const {
	idWinding *w;

	w = new idWinding( numPoints );
	w->numPoints = numPoints;
	memcpy( w->p, p, numPoints * sizeof(p[0]) );
	return w;
}

/*
=============
idWinding::Reverse
=============
*/
idWinding *idWinding::Reverse( void ) const {
	idWinding *w;
	int i;

	w = new idWinding( numPoints );
	w->numPoints = numPoints;
	for ( i = 0; i < numPoints; i++ ) {
		w->p[ numPoints - i - 1 ] = p[i];
	}
	return w;
}

/*
=============
idWinding::ReverseSelf
=============
*/
void idWinding::ReverseSelf( void ) {
	idVec5 v;
	int i;

	for ( i = 0; i < (numPoints>>1); i++ ) {
		v = p[i];
		p[i] = p[numPoints - i - 1];
		p[numPoints - i - 1] = v;
	}
}

/*
=============
idWinding::Check
=============
*/
bool idWinding::Check( bool print ) const {
	int				i, j;
	float			d, edgedist;
	idVec3			dir, edgenormal;
	float			area;
	idPlane			plane;

	if ( numPoints < 3 ) {
		if ( print ) {
			idLib::common->Printf( "idWinding::Check: only %i points.", numPoints );
		}
		return false;
	}
	
	area = GetArea();
	if ( area < 1.0f ) {
		if ( print ) {
			idLib::common->Printf( "idWinding::Check: tiny area: %f", area );
		}
		return false;
	}

	GetPlane( plane );
	
	for ( i = 0; i < numPoints; i++ ) {
		const idVec3 &p1 = p[i].ToVec3();

		// check if the winding is huge
		for ( j = 0; j < 3; j++ ) {
			if ( p1[j] >= MAX_WORLD_COORD || p1[j] <= MIN_WORLD_COORD ) {
				if ( print ) {
					idLib::common->Printf( "idWinding::Check: point %d outside world %c-axis: %f", i, 'X'+j, p1[j] );
				}
				return false;
			}
		}

		j = i + 1 == numPoints ? 0 : i + 1;
		
		// check if the point is on the face plane
		d = p1 * plane.Normal() + plane[3];
		if ( d < -ON_EPSILON || d > ON_EPSILON ) {
			if ( print ) {
				idLib::common->Printf( "idWinding::Check: point %d off plane.", i );
			}
			return false;
		}
	
		// check if the edge isn't degenerate
		const idVec3 &p2 = p[j].ToVec3();
		dir = p2 - p1;
		
		if ( dir.Length() < ON_EPSILON) {
			if ( print ) {
				idLib::common->Printf( "idWinding::Check: edge %d is degenerate.", i );
			}
			return false;
		}

		// check if the winding is convex
		edgenormal = plane.Normal().Cross( dir );
		edgenormal.Normalize();
		edgedist = p1 * edgenormal;
		edgedist += ON_EPSILON;
		
		// all other points must be on front side
		for ( j = 0; j < numPoints; j++ ) {
			if ( j == i ) {
				continue;
			}
			d = p[j].ToVec3() * edgenormal;
			if ( d > edgedist ) {
				if ( print ) {
					idLib::common->Printf( "idWinding::Check: non-convex." );
				}
				return false;
			}
		}
	}
	return true;
}

/*
=============
idWinding::GetArea
=============
*/
float idWinding::GetArea( void ) const {
	int i;
	idVec3 d1, d2, cross;
	float total;

	total = 0.0f;
	for ( i = 2; i < numPoints; i++ ) {
		d1 = p[i-1].ToVec3() - p[0].ToVec3();
		d2 = p[i].ToVec3() - p[0].ToVec3();
		cross = d1.Cross( d2 );
		total += cross.Length();
	}
	return total * 0.5f;
}

/*
=============
idWinding::GetRadius
=============
*/
float idWinding::GetRadius( const idVec3 &center ) const {
	int i;
	float radius, r;
	idVec3 dir;

	radius = 0.0f;
	for ( i = 0; i < numPoints; i++ ) {
		dir = p[i].ToVec3() - center;
		r = dir * dir;
		if ( r > radius ) {
			radius = r;
		}
	}
	return idMath::Sqrt( radius );
}

/*
=============
idWinding::GetCenter
=============
*/
idVec3 idWinding::GetCenter( void ) const {
	int i;
	idVec3 center;

	center.Zero();
	for ( i = 0; i < numPoints; i++ ) {
		center += p[i].ToVec3();
	}
	center *= ( 1.0f / numPoints );
	return center;
}

/*
=============
idWinding::GetPlane
=============
*/
void idWinding::GetPlane( idVec3 &normal, float &dist ) const {
	if ( numPoints < 3 ) {
		normal.Zero();
		dist = 0.0f;
		return;
	}
	auto pnt = [&](int i) { return idVec3d(p[i].ToVec3()); };
	idVec3d directedArea = idVec3d(0.0);
	for (int i = 1; i+1 < numPoints; i++)
		directedArea += (pnt(i+1) - pnt(0)).Cross(pnt(i) - pnt(0));
	directedArea.Normalize();
	normal = idVec3(directedArea);
	dist = (float) pnt(0).Dot(idVec3d(normal));
}

/*
=============
idWinding::GetPlane
=============
*/
void idWinding::GetPlane( idPlane &plane ) const {
	GetPlane(plane.Normal(), plane[3]);
	plane[3] = -plane[3];
}

/*
=============
idWinding::GetBounds
=============
*/
void idWinding::GetBounds( idBounds &bounds ) const {
	int i;

	if ( !numPoints ) {
		bounds.Clear();
		return;
	}

	bounds[0] = bounds[1] = p[0].ToVec3();
	for ( i = 1; i < numPoints; i++ ) {
		if ( p[i].x < bounds[0].x ) {
			bounds[0].x = p[i].x;
		} else if ( p[i].x > bounds[1].x ) {
			bounds[1].x = p[i].x;
		}
		if ( p[i].y < bounds[0].y ) {
			bounds[0].y = p[i].y;
		} else if ( p[i].y > bounds[1].y ) {
			bounds[1].y = p[i].y;
		}
		if ( p[i].z < bounds[0].z ) {
			bounds[0].z = p[i].z;
		} else if ( p[i].z > bounds[1].z ) {
			bounds[1].z = p[i].z;
		}
	}
}

/*
=============
idWinding::RemoveEqualPoints
=============
*/
void idWinding::RemoveEqualPoints( const float epsilon ) {
	int i, j;

	for ( i = 0; i < numPoints; i++ ) {
		if ( (p[i].ToVec3() - p[(i+numPoints-1)%numPoints].ToVec3()).LengthSqr() >= Square( epsilon ) ) {
			continue;
		}
		numPoints--;
		for ( j = i; j < numPoints; j++ ) {
			p[j] = p[j+1];
		}
		i--;
	}
}

/*
=============
idWinding::RemoveColinearPoints
=============
*/
void idWinding::RemoveColinearPoints( const idVec3 &normal, const float epsilon ) {
	int i, j;
	idVec3 edgeNormal;
	float dist;

	if ( numPoints <= 3 ) {
		return;
	}

	for ( i = 0; i < numPoints; i++ ) {

		// create plane through edge orthogonal to winding plane
		edgeNormal = (p[i].ToVec3() - p[(i+numPoints-1)%numPoints].ToVec3()).Cross( normal );
		edgeNormal.Normalize();
		dist = edgeNormal * p[i].ToVec3();

		if ( idMath::Fabs( edgeNormal * p[(i+1)%numPoints].ToVec3() - dist ) > epsilon ) {
			continue;
		}

		numPoints--;
		for ( j = i; j < numPoints; j++ ) {
			p[j] = p[j+1];
		}
		i--;
	}
}

/*
=============
idWinding::AddToConvexHull

  Adds the given winding to the convex hull.
  Assumes the current winding already is a convex hull with three or more points.
=============
*/
void idWinding::AddToConvexHull( const idWinding *winding, const idVec3 &normal, const float epsilon ) {
	int				i, j, k;
	idVec3			dir;
	float			d;
	int				maxPts;
	idVec3 *		hullDirs;
	bool *			hullSide;
	bool			outside;
	int				numNewHullPoints;
	idVec5 *		newHullPoints;

	if ( !winding ) {
		return;
	}

	maxPts = this->numPoints + winding->numPoints;

	if ( !this->EnsureAlloced( maxPts, true ) ) {
		return;
	}

	newHullPoints = (idVec5 *) _alloca( maxPts * sizeof( idVec5 ) );
	hullDirs = (idVec3 *) _alloca( maxPts * sizeof( idVec3 ) );
	hullSide = (bool *) _alloca( maxPts * sizeof( bool ) );

	for ( i = 0; i < winding->numPoints; i++ ) {
		const idVec5 &p1 = winding->p[i];

		// calculate hull edge vectors
		for ( j = 0; j < this->numPoints; j++ ) {
			dir = this->p[ (j + 1) % this->numPoints ].ToVec3() - this->p[ j ].ToVec3();
			dir.Normalize();
			hullDirs[j] = normal.Cross( dir );
		}

		// calculate side for each hull edge
		outside = false;
		for ( j = 0; j < this->numPoints; j++ ) {
			dir = p1.ToVec3() - this->p[j].ToVec3();
			d = dir * hullDirs[j];
			if ( d >= epsilon ) {
				outside = true;
			}
			if ( d >= -epsilon ) {
				hullSide[j] = true;
			} else {
				hullSide[j] = false;
			}
		}

		// if the point is effectively inside, do nothing
		if ( !outside ) {
			continue;
		}

		// find the back side to front side transition
		for ( j = 0; j < this->numPoints; j++ ) {
			if ( !hullSide[ j ] && hullSide[ (j + 1) % this->numPoints ] ) {
				break;
			}
		}
		if ( j >= this->numPoints ) {
			continue;
		}

		// insert the point here
		newHullPoints[0] = p1;
		numNewHullPoints = 1;

		// copy over all points that aren't double fronts
		j = (j+1) % this->numPoints;
		for ( k = 0; k < this->numPoints; k++ ) {
			if ( hullSide[ (j+k) % this->numPoints ] && hullSide[ (j+k+1) % this->numPoints ] ) {
				continue;
			}
			newHullPoints[numNewHullPoints] = this->p[ (j+k+1) % this->numPoints ];
			numNewHullPoints++;
		}

		this->numPoints = numNewHullPoints;
		memcpy( this->p, newHullPoints, numNewHullPoints * sizeof(idVec5) );
	}
}

/*
=============
idWinding::AddToConvexHull

  Add a point to the convex hull.
  The current winding must be convex but may be degenerate and can have less than three points.
=============
*/
void idWinding::AddToConvexHull( const idVec3 &point, const idVec3 &normal, const float epsilon ) {
	int				j, k, numHullPoints;
	idVec3			dir;
	float			d;
	idVec3 *		hullDirs;
	bool *			hullSide;
	idVec5 *		hullPoints;
	bool			outside;

	switch( numPoints ) {
		case 0: {
			p[0] = point;
			numPoints++;
			return;
		}
		case 1: {
			// don't add the same point second
			if ( p[0].ToVec3().Compare( point, epsilon ) ) {
				return;
			}
			p[1].ToVec3() = point;
			numPoints++;
			return;
		}
		case 2: {
			// don't add a point if it already exists
			if ( p[0].ToVec3().Compare( point, epsilon ) || p[1].ToVec3().Compare( point, epsilon ) ) {
				return;
			}
			// if only two points make sure we have the right ordering according to the normal
			dir = point - p[0].ToVec3();
			dir = dir.Cross( p[1].ToVec3() - p[0].ToVec3() );
			if ( dir[0] == 0.0f && dir[1] == 0.0f && dir[2] == 0.0f ) {
				// points don't make a plane
				return;
			}
			if ( dir * normal > 0.0f ) {
				p[2].ToVec3() = point;
			}
			else {
				p[2] = p[1];
				p[1].ToVec3() = point;
			}
			numPoints++;
			return;
		}
	}

	hullDirs = (idVec3 *) _alloca( numPoints * sizeof( idVec3 ) );
	hullSide = (bool *) _alloca( numPoints * sizeof( bool ) );

	// calculate hull edge vectors
	for ( j = 0; j < numPoints; j++ ) {
		dir = p[(j + 1) % numPoints].ToVec3() - p[j].ToVec3();
		hullDirs[j] = normal.Cross( dir );
	}

	// calculate side for each hull edge
	outside = false;
	for ( j = 0; j < numPoints; j++ ) {
		dir = point - p[j].ToVec3();
		d = dir * hullDirs[j];
		if ( d >= epsilon ) {
			outside = true;
		}
		if ( d >= -epsilon ) {
			hullSide[j] = true;
		} else {
			hullSide[j] = false;
		}
	}

	// if the point is effectively inside, do nothing
	if ( !outside ) {
		return;
	}

	// find the back side to front side transition
	for ( j = 0; j < numPoints; j++ ) {
		if ( !hullSide[ j ] && hullSide[ (j + 1) % numPoints ] ) {
			break;
		}
	}
	if ( j >= numPoints ) {
		return;
	}

	hullPoints = (idVec5 *) _alloca( (numPoints+1) * sizeof( idVec5 ) );

	// insert the point here
	hullPoints[0] = point;
	numHullPoints = 1;

	// copy over all points that aren't double fronts
	j = (j+1) % numPoints;
	for ( k = 0; k < numPoints; k++ ) {
		if ( hullSide[ (j+k) % numPoints ] && hullSide[ (j+k+1) % numPoints ] ) {
			continue;
		}
		hullPoints[numHullPoints] = p[ (j+k+1) % numPoints ];
		numHullPoints++;
	}

	if ( !EnsureAlloced( numHullPoints, false ) ) {
		return;
	}
	numPoints = numHullPoints;
	memcpy( p, hullPoints, numHullPoints * sizeof(idVec5) );
}

/*
=============
idWinding::TryMerge
=============
*/
#define	CONTINUOUS_EPSILON	0.005f

idWinding *idWinding::TryMerge( const idWinding &w, const idVec3 &planenormal, int keep ) const {
	idVec3			*p1, *p2, *p3, *p4, *back;
	idWinding		*newf;
	const idWinding	*f1, *f2;
	int				i, j, k, l;
	idVec3			normal, delta;
	float			dot;
	bool			keep1, keep2;

	f1 = this;
	f2 = &w;
	//
	// find a idLib::common edge
	//	
	p1 = p2 = NULL;	// stop compiler warning
	j = 0;
	
	for ( i = 0; i < f1->numPoints; i++ ) {
		p1 = &f1->p[i].ToVec3();
		p2 = &f1->p[(i+1) % f1->numPoints].ToVec3();
		for ( j = 0; j < f2->numPoints; j++ ) {
			p3 = &f2->p[j].ToVec3();
			p4 = &f2->p[(j+1) % f2->numPoints].ToVec3();
			for (k = 0; k < 3; k++ ) {
				if ( idMath::Fabs((*p1)[k] - (*p4)[k]) > 0.1f ) {
					break;
				}
				if ( idMath::Fabs((*p2)[k] - (*p3)[k]) > 0.1f ) {
					break;
				}
			}
			if ( k == 3 ) {
				break;
			}
		}
		if ( j < f2->numPoints ) {
			break;
		}
	}
	
	if ( i == f1->numPoints ) {
		return NULL;			// no matching edges
	}

	//
	// check slope of connected lines
	// if the slopes are colinear, the point can be removed
	//
	back = &f1->p[(i+f1->numPoints-1)%f1->numPoints].ToVec3();
	delta = (*p1) - (*back);
	normal = planenormal.Cross(delta);
	normal.Normalize();
	
	back = &f2->p[(j+2)%f2->numPoints].ToVec3();
	delta = (*back) - (*p1);
	dot = delta * normal;
	if ( dot > CONTINUOUS_EPSILON ) {
		return NULL;			// not a convex polygon
	}

	keep1 = (bool)(dot < -CONTINUOUS_EPSILON);
	
	back = &f1->p[(i+2)%f1->numPoints].ToVec3();
	delta = (*back) - (*p2);
	normal = planenormal.Cross( delta );
	normal.Normalize();

	back = &f2->p[(j+f2->numPoints-1)%f2->numPoints].ToVec3();
	delta = (*back) - (*p2);
	dot = delta * normal;
	if ( dot > CONTINUOUS_EPSILON ) {
		return NULL;			// not a convex polygon
	}

	keep2 = (bool)(dot < -CONTINUOUS_EPSILON);

	//
	// build the new polygon
	//
	newf = new idWinding( f1->numPoints + f2->numPoints );
	
	// copy first polygon
	for ( k = (i+1) % f1->numPoints; k != i; k = (k+1) % f1->numPoints ) {
		if ( !keep && k == (i+1) % f1->numPoints && !keep2 ) {
			continue;
		}
		
		newf->p[newf->numPoints] = f1->p[k];
		newf->numPoints++;
	}
	
	// copy second polygon
	for ( l = (j+1) % f2->numPoints; l != j; l = (l+1) % f2->numPoints ) {
		if ( !keep && l == (j+1) % f2->numPoints && !keep1 ) {
			continue;
		}
		newf->p[newf->numPoints] = f2->p[l];
		newf->numPoints++;
	}

	return newf;
}

/*
=============
idWinding::RemovePoint
=============
*/
void idWinding::RemovePoint( int point ) {
	if ( point < 0 || point >= numPoints ) {
		idLib::common->FatalError( "idWinding::removePoint: point out of range" );
	}
	if ( point < numPoints - 1) {
		memmove(&p[point], &p[point+1], (numPoints - point - 1) * sizeof(p[0]) );
	}
	numPoints--;
}

/*
=============
idWinding::InsertPoint
=============
*/
void idWinding::InsertPoint( const idVec3 &point, int spot ) {
	int i;

	if ( spot > numPoints ) {
		idLib::common->FatalError( "idWinding::insertPoint: spot > numPoints" );
	}

	if ( spot < 0 ) {
		idLib::common->FatalError( "idWinding::insertPoint: spot < 0" );
	}

	EnsureAlloced( numPoints+1, true );
	for ( i = numPoints; i > spot; i-- ) {
		p[i] = p[i-1];
	}
	p[spot] = point;
	numPoints++;
}

/*
=============
idWinding::InsertPointIfOnEdge
=============
*/
bool idWinding::InsertPointIfOnEdge( const idVec3 &point, const idPlane &plane, const float epsilon ) {
	int i;
	float dist, dot;
	idVec3 normal;

	// point may not be too far from the winding plane
	if ( idMath::Fabs( plane.Distance( point ) ) > epsilon ) {
		return false;
	}

	for ( i = 0; i < numPoints; i++ ) {

		// create plane through edge orthogonal to winding plane
		normal = (p[(i+1)%numPoints].ToVec3() - p[i].ToVec3()).Cross( plane.Normal() );
		normal.Normalize();
		dist = normal * p[i].ToVec3();

		if ( idMath::Fabs( normal * point - dist ) > epsilon ) {
			continue;
		}

		normal = plane.Normal().Cross( normal );
		dot = normal * point;

		dist = dot - normal * p[i].ToVec3();

		if ( dist < epsilon ) {
			// if the winding already has the point
			if ( dist > -epsilon ) {
				return false;
			}
			continue;
		}

		dist = dot - normal * p[(i+1)%numPoints].ToVec3();

		if ( dist > -epsilon ) {
			// if the winding already has the point
			if ( dist < epsilon ) {
				return false;
			}
			continue;
		}

		InsertPoint( point, i+1 );
		return true;
	}
	return false;
}

/*
=============
idWinding::IsTiny
=============
*/
#define	EDGE_LENGTH		0.2f

bool idWinding::IsTiny( void ) const {
	int		i;
	float	len;
	idVec3	delta;
	int		edges;

	edges = 0;
	for ( i = 0; i < numPoints; i++ ) {
		delta = p[(i+1)%numPoints].ToVec3() - p[i].ToVec3();
		len = delta.Length();
		if ( len > EDGE_LENGTH ) {
			if ( ++edges == 3 ) {
				return false;
			}
		}
	}
	return true;
}

/*
=============
idWinding::IsHuge
=============
*/
bool idWinding::IsHuge( void ) const {
	int i, j;

	for ( i = 0; i < numPoints; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			if ( p[i][j] <= MIN_WORLD_COORD || p[i][j] >= MAX_WORLD_COORD ) {
				return true;
			}
		}
	}
	return false;
}

/*
=============
idWinding::Print
=============
*/
void idWinding::Print( void ) const {
	int i;

	for ( i = 0; i < numPoints; i++ ) {
		idLib::common->Printf( "(%5.1f, %5.1f, %5.1f)\n", p[i][0], p[i][1], p[i][2] );
	}
}

/*
=============
idWinding::PlaneDistance
=============
*/
float idWinding::PlaneDistance( const idPlane &plane ) const {
	int		i;
	float	d, min, max;

	min = idMath::INFINITY;
	max = -min;
	for ( i = 0; i < numPoints; i++ ) {
		d = plane.Distance( p[i].ToVec3() );
		if ( d < min ) {
			min = d;
			if ( FLOATSIGNBITSET( min ) & FLOATSIGNBITNOTSET( max ) ) {
				return 0.0f;
			}
		}
		if ( d > max ) {
			max = d;
			if ( FLOATSIGNBITSET( min ) & FLOATSIGNBITNOTSET( max ) ) {
				return 0.0f;
			}
		}
	}
	if ( FLOATSIGNBITNOTSET( min ) ) {
		return min;
	}
	if ( FLOATSIGNBITSET( max ) ) {
		return max;
	}
	return 0.0f;
}

/*
=============
idWinding::PlaneSide
=============
*/
int idWinding::PlaneSide( const idPlane &plane, const float epsilon ) const {
	bool	front, back;
	int		i;
	float	d;

	front = false;
	back = false;
	for ( i = 0; i < numPoints; i++ ) {
		d = plane.Distance( p[i].ToVec3() );
		if ( d < -epsilon ) {
			if ( front ) {
				return SIDE_CROSS;
			}
			back = true;
			continue;
		}
		else if ( d > epsilon ) {
			if ( back ) {
				return SIDE_CROSS;
			}
			front = true;
			continue;
		}
	}

	if ( back ) {
		return SIDE_BACK;
	}
	if ( front ) {
		return SIDE_FRONT;
	}
	return SIDE_ON;
}

/*
=============
idWinding::PlanesConcave
=============
*/
#define WCONVEX_EPSILON		0.2f

bool idWinding::PlanesConcave( const idWinding &w2, const idVec3 &normal1, const idVec3 &normal2, float dist1, float dist2 ) const {
	int i;

	// check if one of the points of winding 1 is at the back of the plane of winding 2
	for ( i = 0; i < numPoints; i++ ) {
		if ( normal2 * p[i].ToVec3() - dist2 > WCONVEX_EPSILON ) {
			return true;
		}
	}
	// check if one of the points of winding 2 is at the back of the plane of winding 1
	for ( i = 0; i < w2.numPoints; i++ ) {
		if ( normal1 * w2.p[i].ToVec3() - dist1 > WCONVEX_EPSILON ) {
			return true;
		}
	}

	return false;
}

/*
=============
idWinding::PointInside
=============
*/
bool idWinding::PointInside( const idVec3 &normal, const idVec3 &point, const float epsilon ) const {
	int i;
	idVec3 dir, n, pointvec;

	for ( i = 0; i < numPoints; i++ ) {
		dir = p[(i+1) % numPoints].ToVec3() - p[i].ToVec3();
		pointvec = point - p[i].ToVec3();

		n = dir.Cross( normal );

		if ( pointvec * n < -epsilon ) {
			return false;
		}
	}
	return true;
}
/*
=============
idWinding::PointInsideDst
=============
*/
bool idWinding::PointInsideDst( const idVec3 &normal, const idVec3 &point, const float epsilon ) const {
	int i;
	idVec3 dir, n, pointvec;
	assert( fabs( normal.Length() - 1.0f ) <= 1e-3f );

	for ( i = 0; i < numPoints; i++ ) {
		dir = p[(i+1) % numPoints].ToVec3() - p[i].ToVec3();
		pointvec = point - p[i].ToVec3();

		n = dir.Cross( normal );

		if ( pointvec * n < -epsilon * dir.Length() ) {
			return false;
		}
	}
	return true;
}

/*
=============
idWinding::PointLiesOn
=============
*/
bool idWinding::PointLiesOn( const idVec3 &point, const float epsilon ) const {
	idPlane plane;
	GetPlane(plane);

	int side = plane.Side(point, epsilon);
	if (side != PLANESIDE_ON)
		return false;

	if (!PointInsideDst(plane.Normal(), point, epsilon))
		return false;

	return true;
}

/*
=============
idWinding::LineIntersection
=============
*/
bool idWinding::LineIntersection( const idPlane &windingPlane, const idVec3 &start, const idVec3 &end, bool backFaceCull ) const {
	float front, back, frac;
	idVec3 mid;

	front = windingPlane.Distance( start );
	back = windingPlane.Distance( end );

	// if both points at the same side of the plane
	if ( front < 0.0f && back < 0.0f ) {
		return false;
	}

	if ( front > 0.0f && back > 0.0f ) {
		return false;
	}

	// if back face culled
	if ( backFaceCull && front < 0.0f ) {
		return false;
	}

	// get point of intersection with winding plane
	if ( idMath::Fabs(front - back) < 0.0001f ) {
		mid = end;
	}
	else {
		frac = front / (front - back);
		mid[0] = start[0] + (end[0] - start[0]) * frac;
		mid[1] = start[1] + (end[1] - start[1]) * frac;
		mid[2] = start[2] + (end[2] - start[2]) * frac;
	}

	return PointInside( windingPlane.Normal(), mid, 0.0f );
}

/*
=============
idWinding::RayIntersection
=============
*/
bool idWinding::RayIntersection( const idPlane &windingPlane, const idVec3 &start, const idVec3 &dir, float &scale, bool backFaceCull ) const {
	int i;
	bool side, lastside = false;
	idPluecker pl1, pl2;

	scale = 0.0f;
	pl1.FromRay( start, dir );
	for ( i = 0; i < numPoints; i++ ) {
		pl2.FromLine( p[i].ToVec3(), p[(i+1)%numPoints].ToVec3() );
		side = pl1.PermutedInnerProduct( pl2 ) > 0.0f;
		if ( i && side != lastside ) {
			return false;
		}
		lastside = side;
	}
	if ( !backFaceCull || lastside ) {
		windingPlane.RayIntersection( start, dir, scale );
		return true;
	}
	return false;
}

/*
=================
idWinding::TriangleArea
=================
*/
float idWinding::TriangleArea( const idVec3 &a, const idVec3 &b, const idVec3 &c ) {
	idVec3	v1, v2;
	idVec3	cross;

	v1 = b - a;
	v2 = c - a;
	cross = v1.Cross( v2 );
	return 0.5f * cross.Length();
}
double idWinding::TriangleAreaDbl( const idVec3 &a, const idVec3 &b, const idVec3 &c ) {
	idVec3d v1 = idVec3d(b) - idVec3d(a);
	idVec3d v2 = idVec3d(c) - idVec3d(a);
	idVec3d cross = v1.Cross( v2 );
	return 0.5 * cross.Length();
}


//===============================================================
//
//	idFixedWinding
//
//===============================================================

/*
=============
idFixedWinding::ReAllocate
=============
*/
bool idFixedWinding::ReAllocate( int n, bool keep ) {

	assert( n <= MAX_POINTS_ON_WINDING );

	if ( n > MAX_POINTS_ON_WINDING ) {
		idLib::common->Printf("WARNING: idFixedWinding -> MAX_POINTS_ON_WINDING overflowed\n");
		return false;
	}
	return true;
}

/*
=============
idFixedWinding::Split
=============
*/
int idFixedWinding::Split( idFixedWinding *back, const idPlane &plane, const float epsilon ) {
	int		counts[3];
	float	dists[MAX_POINTS_ON_WINDING+4];
	byte	sides[MAX_POINTS_ON_WINDING+4];
	float	dot;
	int		i, j;
	idVec5 *p1, *p2;
	idVec5	mid;
	idFixedWinding out;

	counts[SIDE_FRONT] = counts[SIDE_BACK] = counts[SIDE_ON] = 0;

	// determine sides for each point
	for ( i = 0; i < numPoints; i++ ) {
		dists[i] = dot = plane.Distance( p[i].ToVec3() );
		if ( dot > epsilon ) {
			sides[i] = SIDE_FRONT;
		} else if ( dot < -epsilon ) {
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}

	if ( !counts[SIDE_BACK] ) {
		if ( !counts[SIDE_FRONT] ) {
			return SIDE_ON;
		}
		else {
			return SIDE_FRONT;
		}
	}
	
	if ( !counts[SIDE_FRONT] ) {
		return SIDE_BACK;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];
	
	out.numPoints = 0;
	back->numPoints = 0;

	for ( i = 0; i < numPoints; i++ ) {
		p1 = &p[i];

		if ( !out.EnsureAlloced( out.numPoints+1, true ) ) {
			return SIDE_FRONT;		// can't split -- fall back to original
		}
		if ( !back->EnsureAlloced( back->numPoints+1, true ) ) {
			return SIDE_FRONT;		// can't split -- fall back to original
		}

		if ( sides[i] == SIDE_ON ) {
			out.p[out.numPoints] = *p1;
			out.numPoints++;
			back->p[back->numPoints] = *p1;
			back->numPoints++;
			continue;
		}
	
		if ( sides[i] == SIDE_FRONT ) {
			out.p[out.numPoints] = *p1;
			out.numPoints++;
		}
		if ( sides[i] == SIDE_BACK ) {
			back->p[back->numPoints] = *p1;
			back->numPoints++;
		}
		
		if ( sides[i+1] == SIDE_ON || sides[i+1] == sides[i] ) {
			continue;
		}
			
		if ( !out.EnsureAlloced( out.numPoints+1, true ) ) {
			return SIDE_FRONT;		// can't split -- fall back to original
		}

		if ( !back->EnsureAlloced( back->numPoints+1, true ) ) {
			return SIDE_FRONT;		// can't split -- fall back to original
		}

		// generate a split point
		j = i + 1;
		if ( j >= numPoints ) {
			p2 = &p[0];
		}
		else {
			p2 = &p[j];
		}

		dot = dists[i] / (dists[i] - dists[i+1]);
		for ( j = 0; j < 3; j++ ) {
			// avoid round off error when possible
			if ( plane.Normal()[j] == 1.0f ) {
				mid[j] = plane.Dist();
			} else if ( plane.Normal()[j] == -1.0f ) {
				mid[j] = -plane.Dist();
			} else {
				mid[j] = (*p1)[j] + dot * ( (*p2)[j] - (*p1)[j] );
			}
		}
		mid.s = p1->s + dot * ( p2->s - p1->s );
		mid.t = p1->t + dot * ( p2->t - p1->t );
			
		out.p[out.numPoints] = mid;
		out.numPoints++;
		back->p[back->numPoints] = mid;
		back->numPoints++;
	}
	for ( i = 0; i < out.numPoints; i++ ) {
		p[i] = out.p[i];
	}
	numPoints = out.numPoints;

	return SIDE_CROSS;
}
