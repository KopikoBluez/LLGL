/*
 * GLVertexArrayObject.h
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#ifndef LLGL_GL_VERTEX_ARRAY_OBJECT_H
#define LLGL_GL_VERTEX_ARRAY_OBJECT_H


#include "../OpenGL.h"
#include <LLGL/Container/ArrayView.h>


namespace LLGL
{


struct VertexAttribute;

// Wrapper class for an OpenGL Vertex-Array-Object (VAO), for GL 3.0+.
class GLVertexArrayObject
{

    public:

        GLVertexArrayObject();
        ~GLVertexArrayObject();

        // Builds the specified attribute using a 'glVertexAttrib*Pointer' function.
        void BuildVertexLayout(const ArrayView<VertexAttribute>& attributes);

        // Returns the ID of the hardware vertex-array-object (VAO)
        inline GLuint GetID() const
        {
            return id_;
        }

    private:

        void BuildVertexAttribute(const VertexAttribute& attribute);

    private:

        GLuint id_ = 0; //!< Vertex array object ID.

};


} // /namespace LLGL


#endif



// ================================================================================
