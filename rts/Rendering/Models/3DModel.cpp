/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "StdAfx.h"

#include <algorithm>
#include <cctype>
#include "mmgr.h"

#include "3DModel.h"
#include "3DOParser.h"
#include "S3OParser.h"
#include "Rendering/FarTextureHandler.h"
#include "Rendering/GL/myGL.h"
#include "Sim/Misc/CollisionVolume.h"
#include "System/Exceptions.h"
#include "System/Util.h"
#include "System/LogOutput.h"


//////////////////////////////////////////////////////////////////////
// S3DModel
//

S3DModelPiece* S3DModel::FindPiece( std::string name )
{
    const ModelPieceMap::const_iterator it = pieces.find(name);
    if (it != pieces.end()) return it->second;
    return NULL;
}


//////////////////////////////////////////////////////////////////////
// S3DModelPiece
//

void S3DModelPiece::DrawStatic() const
{
	glPushMatrix();
	glTranslatef(offset.x, offset.y, offset.z);
	glCallList(dispListID);
	for (std::vector<S3DModelPiece*>::const_iterator ci = childs.begin(); ci != childs.end(); ++ci) {
		(*ci)->DrawStatic();
	}
	glPopMatrix();
}


S3DModelPiece::~S3DModelPiece()
{
	glDeleteLists(dispListID, 1);
	delete colvol;
}



/******************************************************************************/
/******************************************************************************/
//
//  LocalModel
//

LocalModel::~LocalModel()
{
	// delete the local piece copies
	for (std::vector<LocalModelPiece*>::iterator pi = pieces.begin(); pi != pieces.end(); ++pi) {
		delete (*pi)->GetCollisionVolume();
		delete (*pi);
	}
	pieces.clear();
}


void LocalModel::SetLODCount(unsigned int count)
{
	lodCount = count;
	pieces[0]->SetLODCount(count);
}


void LocalModel::ApplyRawPieceTransform(int piecenum) const
{
	pieces[piecenum]->ApplyTransform();
}


float3 LocalModel::GetRawPiecePos(int piecenum) const
{
	return pieces[piecenum]->GetPos();
}


CMatrix44f LocalModel::GetRawPieceMatrix(int piecenum) const
{
	return pieces[piecenum]->GetMatrix();
}




void LocalModel::GetRawEmitDirPos(int piecenum, float3 &pos, float3 &dir) const
{
	pieces[piecenum]->GetEmitDirPos(pos, dir);
}


//Only useful for special pieces used for emit-sfx
float3 LocalModel::GetRawPieceDirection(int piecenum) const
{
	return pieces[piecenum]->GetDirection();
}



void LocalModel::CreatePieces(S3DModelPiece* mpParent, unsigned int* pieceNum) {
	LocalModelPiece* lmpParent = pieces[*pieceNum];
	LocalModelPiece* lmpChild = NULL;

	lmpParent->Init(mpParent);
	lmpParent->SetCollisionVolume(new CollisionVolume(mpParent->GetCollisionVolume()));

	for (unsigned int i = 0; i < mpParent->GetChildCount(); i++) {
		lmpChild = pieces[ ++(*pieceNum) ];

		lmpChild->SetParent(lmpParent);
		lmpParent->AddChild(lmpChild);

		CreatePieces(mpParent->GetChild(i), pieceNum);
	}
}



/******************************************************************************/
/******************************************************************************/
//
//  LocalModelPiece
//

static const float RADTOANG  = 180 / PI;

void LocalModelPiece::Draw() const
{
	if (!visible && childs.size()==0)
		return;

	glPushMatrix();

	if (pos.x || pos.y || pos.z) { glTranslatef(pos.x, pos.y, pos.z); }
	if (rot[1]) { glRotatef(rot[1] * RADTOANG, 0.0f, 1.0f, 0.0f); }
	if (rot[0]) { glRotatef(rot[0] * RADTOANG, 1.0f, 0.0f, 0.0f); }
	if (rot[2]) { glRotatef(rot[2] * RADTOANG, 0.0f, 0.0f, 1.0f); }

	if (visible)
		glCallList(dispListID);

	for (unsigned int i = 0; i < childs.size(); i++) {
		childs[i]->Draw();
	}

	glPopMatrix();
}


void LocalModelPiece::DrawLOD(unsigned int lod) const
{
	if (!visible && childs.empty())
		return;

	glPushMatrix();

	if (pos.x || pos.y || pos.z) { glTranslatef(pos.x, pos.y, pos.z); }
	if (rot[1]) { glRotatef(rot[1] * RADTOANG, 0.0f, 1.0f, 0.0f); }
	if (rot[0]) { glRotatef(rot[0] * RADTOANG, 1.0f, 0.0f, 0.0f); }
	if (rot[2]) { glRotatef(rot[2] * RADTOANG, 0.0f, 0.0f, 1.0f); }

	if (visible)
		glCallList(lodDispLists[lod]);

	for (unsigned int i = 0; i < childs.size(); i++) {
		childs[i]->DrawLOD(lod);
	}

	glPopMatrix();
}


void LocalModelPiece::ApplyTransform() const
{
	if (parent) {
		parent->ApplyTransform();
	}

	if (pos.x || pos.y || pos.z) { glTranslatef(pos.x, pos.y, pos.z); }
	if (rot[1]) { glRotatef(rot[1] * RADTOANG, 0.0f, 1.0f, 0.0f); }
	if (rot[0]) { glRotatef(rot[0] * RADTOANG, 1.0f, 0.0f, 0.0f); }
	if (rot[2]) { glRotatef(rot[2] * RADTOANG, 0.0f, 0.0f, 1.0f); }
}


void LocalModelPiece::GetPiecePosIter(CMatrix44f* mat) const
{
	if (parent) {
		parent->GetPiecePosIter(mat);
	}

	if (pos.x || pos.y || pos.z) { mat->Translate(pos.x, pos.y, pos.z); }
	if (rot[1]) { mat->RotateY(-rot[1]); }
	if (rot[0]) { mat->RotateX(-rot[0]); }
	if (rot[2]) { mat->RotateZ(-rot[2]); }
}


void LocalModelPiece::SetLODCount(unsigned int count)
{
	const unsigned int oldCount = lodDispLists.size();

	lodDispLists.resize(count);
	for (unsigned int i = oldCount; i < count; i++) {
		lodDispLists[i] = 0;
	}

	for (unsigned int i = 0; i < childs.size(); i++) {
		childs[i]->SetLODCount(count);
	}
}


#if defined(USE_GML) && defined(__GNUC__) && (__GNUC__ == 4)
// This is supposed to fix some GCC crashbug related to threading
// The MOVAPS SSE instruction is otherwise getting misaligned data
__attribute__ ((force_align_arg_pointer))
#endif
float3 LocalModelPiece::GetPos() const
{
	CMatrix44f mat;
	GetPiecePosIter(&mat);

	mat.Translate(original->GetPosOffset());

	// we use a 'right' vector, and the positive x axis points to the left
	float3 pos = mat.GetPos();
	pos.x = -pos.x;
	return pos;
}


CMatrix44f LocalModelPiece::GetMatrix() const
{
	CMatrix44f mat;
	GetPiecePosIter(&mat);

	return mat;
}


float3 LocalModelPiece::GetDirection() const
{
	const S3DModelPiece* piece = original;
	const unsigned int count = piece->GetVertexCount();
	if (count < 2) {
		return float3(1.0f, 1.0f, 1.0f);
	}
	return (piece->GetVertexPos(0) - piece->GetVertexPos(1));
}


bool LocalModelPiece::GetEmitDirPos(float3& pos, float3& dir) const
{
	CMatrix44f mat;
	GetPiecePosIter(&mat);

	const S3DModelPiece* piece = original;

	if (piece == NULL)
		return false;

	const unsigned int count = piece->GetVertexCount();

	if (count == 0) {
		pos = mat.GetPos();
		dir = mat.Mul(float3(0.0f, 0.0f, 1.0f)) - pos;
	} else if (count == 1) {
		pos = mat.GetPos();
		dir = mat.Mul(piece->GetVertexPos(0)) - pos;
	} else if (count >= 2) {
		float3 p1 = mat.Mul(piece->GetVertexPos(0));
		float3 p2 = mat.Mul(piece->GetVertexPos(1));

		pos = p1;
		dir = p2 - p1;
	} else {
		return false;
	}

	// we use a 'right' vector, and the positive x axis points to the left
	pos.x = -pos.x;
	dir.x = -dir.x;

	return true;
}

/******************************************************************************/
/******************************************************************************/
