/*
 *  GLClientAndServerBuffer.cpp
 *  FreeOrion
 *
 *  Created by Rainer Kupke on 06.02.11.
 *  Copyright 2011. All rights reserved.
 *
 */

#ifdef FREEORION_WIN32
#include <GL/glew.h>
#endif

#include "GLClientAndServerBuffer.h"
#include "../client/human/HumanClientApp.h"


///////////////////////////////////////////////////////////////////////////
// implementation for GLBufferBase
///////////////////////////////////////////////////////////////////////////

GLBufferBase::GLBufferBase() :
b_name(0)
{}

GLBufferBase::~GLBufferBase()
{
    dropServerBuffer();
}

void GLBufferBase::dropServerBuffer(void)
{
    if (b_name) 
    {
        glDeleteBuffers(1, &b_name);
        b_name = 0;
    }
}

void GLBufferBase::harmonizeBufferType(GLBufferBase& other)
{
    if (b_name && other.b_name) return; // OK, both have server buffer
    
    if (b_name || other.b_name)         // NOT OK, only one has server buffer, drop buffer
    {
        dropServerBuffer();
        other.dropServerBuffer();
    }
}

///////////////////////////////////////////////////////////////////////////
// implementation for GLClientAndServerBufferBase<vtype> template
///////////////////////////////////////////////////////////////////////////

template <class vtype> 
GLClientAndServerBufferBase<vtype>::GLClientAndServerBufferBase(std::size_t elementsPerItem) :
GLBufferBase(),
b_data(),
b_size(0),
b_elementsPerItem(elementsPerItem)
{}

template <class vtype> 
std::size_t GLClientAndServerBufferBase<vtype>::size() const
{
    return b_size;
}

template <class vtype> 
void GLClientAndServerBufferBase<vtype>::store(vtype item)
{
    b_data.push_back(item);
    b_size=b_data.size() / b_elementsPerItem;
}

template <class vtype> 
void GLClientAndServerBufferBase<vtype>::store(vtype item1,vtype item2)
{
    b_data.push_back(item1);
    b_data.push_back(item2);
    b_size=b_data.size() / b_elementsPerItem;
}

template <class vtype> 
void GLClientAndServerBufferBase<vtype>::store(vtype item1,vtype item2,vtype item3)
{
    b_data.push_back(item1);
    b_data.push_back(item2);
    b_data.push_back(item3);
    b_size=b_data.size() / b_elementsPerItem;
}

template <class vtype> 
void GLClientAndServerBufferBase<vtype>::store(vtype item1,vtype item2,vtype item3,vtype item4)
{
    b_data.push_back(item1);
    b_data.push_back(item2);
    b_data.push_back(item3);
    b_data.push_back(item4);
    b_size=b_data.size() / b_elementsPerItem;
}

template <class vtype> 
void GLClientAndServerBufferBase<vtype>::createServerBuffer(void)
{
    if (HumanClientApp::GetApp()->GLVersion() >= 1.5f && !b_data.empty())
    {
        glGenBuffers(1, &b_name);
        if (!b_name) return;
        glBindBuffer(GL_ARRAY_BUFFER, b_name);
        glBufferData(GL_ARRAY_BUFFER, 
                     b_data.size() * sizeof(vtype),
                     &b_data[0],
                     GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

template <class vtype> void GLClientAndServerBufferBase<vtype>::clear(void)
{
    dropServerBuffer();
    
    b_size=0;
    b_data.clear();
}

///////////////////////////////////////////////////////////////////////////
// implementation for GLRGBAColorBuffer
///////////////////////////////////////////////////////////////////////////

template class GLClientAndServerBufferBase<unsigned char>;

GLRGBAColorBuffer::GLRGBAColorBuffer() :
GLClientAndServerBufferBase<unsigned char>(4)
{}

void GLRGBAColorBuffer::activate(void) const
{
    if (b_name)
    {
        glBindBuffer(GL_ARRAY_BUFFER, b_name);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);    
    }
    else
    {
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, &b_data[0]);
    }
}

///////////////////////////////////////////////////////////////////////////
// implementation for GL2DVertexBuffer
///////////////////////////////////////////////////////////////////////////

template class GLClientAndServerBufferBase<float>;

GL2DVertexBuffer::GL2DVertexBuffer () :
GLClientAndServerBufferBase<float>(2)
{}

void GL2DVertexBuffer::activate(void) const
{
    if (b_name)
    {
        glBindBuffer(GL_ARRAY_BUFFER, b_name);
        glVertexPointer(2, GL_FLOAT, 0, 0);    
    }
    else
    {
        glVertexPointer(2, GL_FLOAT, 0, &b_data[0]);
    }
}

///////////////////////////////////////////////////////////////////////////
// implementation for GLTexCoordBuffer
///////////////////////////////////////////////////////////////////////////

GLTexCoordBuffer::GLTexCoordBuffer () :
GLClientAndServerBufferBase<float>(2)
{}

void GLTexCoordBuffer::activate(void) const
{
    if (b_name)
    {
        glBindBuffer(GL_ARRAY_BUFFER, b_name);
        glTexCoordPointer(2, GL_FLOAT, 0, 0);    
    }
    else
    {
        glTexCoordPointer(2, GL_FLOAT, 0, &b_data[0]);
    }
}
