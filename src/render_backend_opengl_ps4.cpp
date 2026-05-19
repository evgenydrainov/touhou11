#include "render_backend_opengl.h"
#include "game_assets.h"
#include "stb_sprintf.h"
#include "debug.h"

#define GLCheck(code) \
	{ \
		code; \
		if (int error = glGetError()) \
		{ \
			LogError("GL ERROR %s (error %d)", #code, error); \
			for (;;) {} \
		} \
	}

struct OrbisShaderBlob
{
	char* ident;
	unsigned char hash[16];
	uint64_t len;
	unsigned char* code;
};

extern "C" OrbisShaderBlob scePrecompiledShaderEntries[];

static OrbisShaderBlob *
PlatformPS4FetchShaderByName(const char* shaderName)
{
	for (OrbisShaderBlob* entry = scePrecompiledShaderEntries;
		 entry->ident != nullptr;
		 ++entry)
	{
		if (StrCmp(shaderName, entry->ident) == 0)
		{
			LogInfo("Found shader %s at %p of size %llu", shaderName, entry->code, entry->len);
			return entry;
		}
	}

	LogError("Unable to find shader %s", shaderName);
	return nullptr;
}

static u32
PS4LoadPreloadedShader(RenderBackendOpenGLData *data)
{
	auto vert = PlatformPS4FetchShaderByName("texmap/v_2.vert");
	auto frag = PlatformPS4FetchShaderByName("texmap/f_2.frag");

	u32 fragment_id;
	u32 vertex_id;
	u32 program;

	GLCheck(fragment_id = glCreateShader(GL_FRAGMENT_SHADER));
	GLCheck(vertex_id = glCreateShader(GL_VERTEX_SHADER));

	GLCheck(program = glCreateProgram());

	GLCheck(glShaderBinary(1, &vertex_id, 0, vert->code, vert->len));
	GLCheck(glShaderBinary(1, &fragment_id, 0, frag->code, frag->len));

	GLCheck(glAttachShader(program, vertex_id));
	GLCheck(glAttachShader(program, fragment_id));

	GLCheck(glLinkProgram(program));

	GLCheck(glDeleteShader(vertex_id));
	GLCheck(glDeleteShader(fragment_id));

	int a_vertex;
	GLCheck(a_vertex = glGetAttribLocation(program, "a_vertex"));
	LogInfo("a_vertex=%d", a_vertex);

	return program;
}

void
OpenGLRenderBackendInit(RenderBackend *backend,
						OpenGLContextVersion gl_version,
						Arena *arena,
						OpenGLFunctions *gl,
						int maxNumQuads,
						int mainFrameBufferWidth, int mainFrameBufferHeight)
{
	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;

	usize arenaSavePos = arena->pos;

	data->gl = gl;
	data->gl_version = gl_version;
	data->mainFrameBufferWidth = mainFrameBufferWidth;
	data->mainFrameBufferHeight = mainFrameBufferHeight;
	data->renderToMainFramebuffer = false;

	backend->DrawQuads = OpenGLRenderBackendDrawQuads;
	backend->DrawTriangles3D = OpenGLRenderBackendDrawTriangles3D;
	backend->DrawCircles = OpenGLRenderBackendDrawCircles;
	backend->UploadTextureAsset = OpenGLRenderBackendUploadTextureAsset;
	backend->Clear = OpenGLRenderBackendClear;
	backend->SetViewport = OpenGLRenderBackendSetViewport;
	backend->SetUniform = OpenGLRenderBackendSetUniform;
	backend->OnFullscreenChanged = OpenGLRenderBackendOnFullscreenChanged;

	data->numQuadIndices = 6*maxNumQuads;
	data->vertexBufferSize = sizeof(RenderVertex2D)*maxNumQuads*4;

	if (gl_version != OpenGLContextVersion_1_1)
	{
		u16 *indices = PushArray(arena, data->numQuadIndices, u16);

		u16 offset = 0;
		for (int i = 0; i < data->numQuadIndices; i += 6)
		{
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;

			indices[i + 3] = offset + 2;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 0;

			offset += 4;
		}

		GLCheck(gl->GenBuffers(1, &data->quadIndexBuffer));
		GLCheck(gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, data->quadIndexBuffer));
		GLCheck(gl->BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0])*data->numQuadIndices, indices, GL_STATIC_DRAW));

		GLCheck(gl->GenBuffers(1, &data->vertexBuffer));
		GLCheck(gl->BindBuffer(GL_ARRAY_BUFFER, data->vertexBuffer));
		GLCheck(gl->BufferData(GL_ARRAY_BUFFER, data->vertexBufferSize, nullptr, GL_DYNAMIC_DRAW));

		data->shaders[ShaderIndex_Normal]   = PS4LoadPreloadedShader(data);
		data->shaders[ShaderIndex_Normal3D] = data->shaders[ShaderIndex_Normal];
		data->shaders[ShaderIndex_Fog3D]    = data->shaders[ShaderIndex_Normal];
		data->shaders[ShaderIndex_Circle]   = data->shaders[ShaderIndex_Normal];

		if (data->renderToMainFramebuffer)
		{
			GLCheck(gl->GenTextures(1, &data->mainFrameBufferTexture));
			GLCheck(gl->BindTexture(GL_TEXTURE_2D, data->mainFrameBufferTexture));

			GLCheck(gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
			GLCheck(gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
			GLCheck(gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			GLCheck(gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

			GLCheck(gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mainFrameBufferWidth, mainFrameBufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));

			GLCheck(gl->GenFramebuffers(1, &data->mainFrameBuffer));
			GLCheck(gl->BindFramebuffer(GL_FRAMEBUFFER, data->mainFrameBuffer));
			GLCheck(gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, data->mainFrameBufferTexture, 0));
		}
	}

	LogInfo("OpenGLRenderBackendInit success");

	arena->pos = arenaSavePos;
}

static u32
GetGLFilter(RenderFilter filter)
{
	switch (filter)
	{
		case RenderFilter_Nearest: return GL_NEAREST;
		case RenderFilter_Linear: return GL_LINEAR;
		case RenderFilter_LinearMipMapLinear: return GL_LINEAR_MIPMAP_LINEAR;
	}

	Assert(false);
	return 0;
}

static u32
GetGLBlendFactor(BlendFactor factor)
{
	switch (factor)
	{
		case BlendFactor_One: return GL_ONE;
		case BlendFactor_SrcAlpha: return GL_SRC_ALPHA;
		case BlendFactor_InvSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
	}

	Assert(false);
	return 0;
}

static void
OpenGLRenderBackendSetVertexLayout(RenderBackend *backend,
								   RenderVertex2D *vertices)
{
	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;
	OpenGLFunctions *gl = data->gl;

	if (data->gl_version == OpenGLContextVersion_1_1)
	{
		/*gl->EnableClientState(GL_VERTEX_ARRAY);
		gl->VertexPointer(2, GL_FLOAT, sizeof(RenderVertex2D), (u8*)vertices + OffsetOf(RenderVertex2D, x));

		gl->EnableClientState(GL_TEXTURE_COORD_ARRAY);
		gl->TexCoordPointer(2, GL_FLOAT, sizeof(RenderVertex2D), (u8*)vertices + OffsetOf(RenderVertex2D, u));

		gl->EnableClientState(GL_COLOR_ARRAY);
		gl->ColorPointer(4, GL_UNSIGNED_BYTE, sizeof(RenderVertex2D), (u8*)vertices + OffsetOf(RenderVertex2D, color));*/
	}
	else
	{
		/*gl->VertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(RenderVertex2D), (void*)OffsetOf(RenderVertex2D, x));
		gl->EnableVertexAttribArray(0);

		gl->VertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(RenderVertex2D), (void*)OffsetOf(RenderVertex2D, u));
		gl->EnableVertexAttribArray(1);

		gl->VertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, true, sizeof(RenderVertex2D), (void*)OffsetOf(RenderVertex2D, color));
		gl->EnableVertexAttribArray(2);*/

		GLCheck(glVertexAttribPointer(0, 4, GL_FLOAT, false, sizeof(RenderVertex2D), nullptr));
		GLCheck(glEnableVertexAttribArray(0));
	}
}

static void
OpenGLRenderBackendSetVertexLayout(RenderBackend *backend,
								   RenderVertex3D *vertices)
{
	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;
	OpenGLFunctions *gl = data->gl;

	if (data->gl_version == OpenGLContextVersion_1_1)
	{
		/*gl->EnableClientState(GL_VERTEX_ARRAY);
		gl->VertexPointer(3, GL_FLOAT, sizeof(RenderVertex3D), (u8*)vertices + OffsetOf(RenderVertex3D, x));

		gl->EnableClientState(GL_TEXTURE_COORD_ARRAY);
		gl->TexCoordPointer(2, GL_FLOAT, sizeof(RenderVertex3D), (u8*)vertices + OffsetOf(RenderVertex3D, u));

		gl->EnableClientState(GL_COLOR_ARRAY);
		gl->ColorPointer(4, GL_UNSIGNED_BYTE, sizeof(RenderVertex3D), (u8*)vertices + OffsetOf(RenderVertex3D, color));*/
	}
	else
	{
		/*gl->VertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(RenderVertex3D), (void*)OffsetOf(RenderVertex3D, x));
		gl->EnableVertexAttribArray(0);

		gl->VertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(RenderVertex3D), (void*)OffsetOf(RenderVertex3D, u));
		gl->EnableVertexAttribArray(1);

		gl->VertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, true, sizeof(RenderVertex3D), (void*)OffsetOf(RenderVertex3D, color));
		gl->EnableVertexAttribArray(2);*/
	}
}

template <typename T>
static void
OpenGLRenderBackendDrawPrimitives(RenderBackend *backend,
								  u32 primitiveType,
								  u32 textureID,
								  T *vertices,
								  int numVertices)
{
	TIMED_FUNCTION();

	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;
	OpenGLFunctions *gl = data->gl;

	//LogInfo("HERE 1");

	usize vertexBufferSize = sizeof(vertices[0])*numVertices;
	if (vertexBufferSize <= data->vertexBufferSize)
	{
		GLCheck(gl->UseProgram(data->shaders[backend->shaderIndex]));

		{
			int location;
			GLCheck(location = gl->GetUniformLocation(data->shaders[backend->shaderIndex], "u_modelViewMatrix"));
			if (location != -1)
			{
				GLCheck(gl->UniformMatrix4fv(location, 1, false, &backend->modelView.e[0]));
			}
		}

		{
			int location;
			GLCheck(location = gl->GetUniformLocation(data->shaders[backend->shaderIndex], "u_projectionMatrix"));
			if (location != -1)
			{
				GLCheck(gl->UniformMatrix4fv(location, 1, false, &backend->projection.e[0]));
			}
		}

		{
			int location;
			GLCheck(location = gl->GetUniformLocation(data->shaders[backend->shaderIndex], "u_textureSpaceMatrix"));
			if (location != -1)
			{
				mat4 mat = Matrix4Identity();
				GLCheck(gl->UniformMatrix4fv(location, 1, false, &mat.e[0]));
			}
		}

		{
			int location;
			GLCheck(location = gl->GetUniformLocation(data->shaders[backend->shaderIndex], "u_opacity"));
			if (location != -1)
			{
				float opacity = 1.0f;
				GLCheck(gl->Uniform1fv(location, 1, &opacity));
			}
		}

		OpenGLRenderBackendSetVertexLayout(backend, vertices);

		GLCheck(glActiveTexture(GL_TEXTURE0));
		GLCheck(gl->BindTexture(GL_TEXTURE_2D, textureID));

		{
			int location;
			GLCheck(location = gl->GetUniformLocation(data->shaders[backend->shaderIndex], "s_sampler"));
			if (location != -1)
			{
				GLCheck(glUniform1i(location, 0));
			}
		}

		GLCheck(gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GetGLFilter(backend->minFilter)));
		GLCheck(gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GetGLFilter(backend->magFilter)));

		//LogInfo("HERE 2");

		if (backend->enableBlending)
		{
			GLCheck(gl->Enable(GL_BLEND));
			GLCheck(gl->BlendFunc(GetGLBlendFactor(backend->srcBlendFactor), GetGLBlendFactor(backend->destBlendFactor)));
		}
		else
		{
			GLCheck(gl->Disable(GL_BLEND));
		}

		//LogInfo("HERE 3");

		GLCheck(gl->Disable(GL_CULL_FACE));

		//LogInfo("HERE 4");

		if (data->gl_version == OpenGLContextVersion_1_1)
		{
			/*gl->MatrixMode(GL_MODELVIEW);
			gl->LoadMatrixf(&backend->modelView.e[0]);

			gl->MatrixMode(GL_PROJECTION);
			gl->LoadMatrixf(&backend->projection.e[0]);

			gl->Enable(GL_TEXTURE_2D);

			OpenGLRenderBackendSetVertexLayout(backend, vertices);

			gl->DrawArrays(primitiveType, 0, numVertices);

			backend->currNumDrawCalls++;*/
		}
		else
		{
			GLCheck(gl->BindBuffer(GL_ARRAY_BUFFER, data->vertexBuffer));
			GLCheck(gl->BufferSubData(GL_ARRAY_BUFFER, 0, vertexBufferSize, vertices));

			//LogInfo("HERE 5");

			//LogInfo("HERE 6");

			

			//LogInfo("HERE 7");

			if (primitiveType == GL_QUADS)
			{
				int numQuadIndices = numVertices/4*6;

				if (numQuadIndices <= data->numQuadIndices)
				{
					GLCheck(gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, data->quadIndexBuffer));
					GLCheck(gl->DrawElements(GL_TRIANGLES, numQuadIndices, GL_UNSIGNED_SHORT, nullptr));

					//LogInfo("HERE 8");

					backend->currNumDrawCalls++;
				}
				else
				{
					LogWarn("numQuadIndices is too big");
				}
			}
			else
			{
				GLCheck(gl->DrawArrays(primitiveType, 0, numVertices));

				backend->currNumDrawCalls++;
			}
		}
	}
	else
	{
		LogWarn("vertexBufferSize is too big");
	}
}

RENDER_BACKEND_DRAW_QUADS(OpenGLRenderBackendDrawQuads)
{
	u32 textureID = (u32)(usize)texture->handle;

	OpenGLRenderBackendDrawPrimitives(backend, GL_QUADS, textureID, vertices, num_vertices);
}

RENDER_BACKEND_DRAW_TRIANGLES_3D(OpenGLRenderBackendDrawTriangles3D)
{
	u32 textureID = (u32)(usize)texture->handle;

	//OpenGLRenderBackendDrawPrimitives(backend, GL_TRIANGLES, textureID, vertices, num_vertices);
}

RENDER_BACKEND_DRAW_CIRCLES(OpenGLRenderBackendDrawCircles)
{
	u32 textureID = (u32)(usize)texture->handle;

	ShaderIndex saveShaderIndex = backend->shaderIndex;
	backend->shaderIndex = ShaderIndex_Circle;

	//OpenGLRenderBackendDrawPrimitives(backend, GL_QUADS, textureID, vertices, num_vertices);

	backend->shaderIndex = saveShaderIndex;
}

RENDER_BACKEND_UPLOAD_TEXTURE_ASSET(OpenGLRenderBackendUploadTextureAsset)
{
	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;
	OpenGLFunctions *gl = data->gl;

	u32 id;
	GLCheck(gl->GenTextures(1, &id));
	GLCheck(gl->BindTexture(GL_TEXTURE_2D, id));
	GLCheck(gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT /*GL_CLAMP_TO_EDGE*/));
	GLCheck(gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT /*GL_CLAMP_TO_EDGE*/));
	GLCheck(gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->width, texture->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture->pixels));

	if (texture->wantMipMap)
	{
		/*if (gl->GenerateMipmap)
		{
			gl->GenerateMipmap(GL_TEXTURE_2D);
		}*/
	}

	texture->handle = (void *)(usize)id;

	return true;
}

RENDER_BACKEND_CLEAR(OpenGLRenderBackendClear)
{
	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;
	OpenGLFunctions *gl = data->gl;

	GLCheck(gl->ClearColor(r, g, b, a));
	GLCheck(gl->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
}

RENDER_BACKEND_SET_VIEWPORT(OpenGLRenderBackendSetViewport)
{
	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;
	OpenGLFunctions *gl = data->gl;

	GLCheck(gl->Viewport(x, backend->targetHeight - height - y, width, height));
}

RENDER_BACKEND_SET_UNIFORM(OpenGLRenderBackendSetUniform)
{
	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;
	OpenGLFunctions *gl = data->gl;

	int location;
	GLCheck(location = gl->GetUniformLocation(data->shaders[backend->shaderIndex], name));
	if (location != -1)
	{
		GLCheck(gl->UseProgram(data->shaders[backend->shaderIndex]));

		switch (type)
		{
			case ShaderUniformType_F32:
			{
				GLCheck(gl->Uniform1fv(location, 1, (f32 *)value));
			} break;

			case ShaderUniformType_Vec4:
			{
				GLCheck(gl->Uniform4fv(location, 1, (f32 *)value));
			} break;
		}
	}
}

RENDER_BACKEND_ON_FULLSCREEN_CHANGED(OpenGLRenderBackendOnFullscreenChanged)
{
	// nothing to do
}

bool
OpenGLRenderBackendPrepareDraw(RenderBackend *backend)
{
	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;
	OpenGLFunctions *gl = data->gl;

	backend->numDrawCalls = backend->currNumDrawCalls;
	backend->currNumDrawCalls = 0;

	if (data->renderToMainFramebuffer)
	{
		GLCheck(gl->BindFramebuffer(GL_FRAMEBUFFER, data->mainFrameBuffer));

		backend->targetWidth = data->mainFrameBufferWidth;
		backend->targetHeight = data->mainFrameBufferHeight;
	}
	else
	{
		GLCheck(gl->BindFramebuffer(GL_FRAMEBUFFER, 0));

		backend->targetWidth = backend->windowWidth;
		backend->targetHeight = backend->windowHeight;
	}

	return true;
}

void
OpenGLRenderBackendPresent(RenderBackend *backend)
{
	TIMED_FUNCTION();

	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;
	OpenGLFunctions *gl = data->gl;

	if (data->renderToMainFramebuffer)
	{
		backend->projection = Matrix4Ortho(0.0f, (f32)backend->windowWidth, (f32)backend->windowHeight, 0.0f);

		Viewport viewport = GetViewportKeepAspect(backend->windowWidth, backend->windowHeight,
												  data->mainFrameBufferWidth, data->mainFrameBufferHeight,
												  0, 0, data->mainFrameBufferWidth, data->mainFrameBufferHeight);

		f32 x1 = (f32)viewport.x;
		f32 y1 = (f32)viewport.y;

		f32 x2 = (f32)(viewport.x + viewport.width);
		f32 y2 = (f32)(viewport.y + viewport.height);

		RenderVertex2D vertices[] = {
			{x1, y1, 0.0f, 1.0f, 0xffffffff},
			{x2, y1, 1.0f, 1.0f, 0xffffffff},
			{x2, y2, 1.0f, 0.0f, 0xffffffff},
			{x1, y2, 0.0f, 0.0f, 0xffffffff},
		};

		GLCheck(gl->BindFramebuffer(GL_FRAMEBUFFER, 0));
		GLCheck(gl->Viewport(0, 0, backend->windowWidth, backend->windowHeight));

		GLCheck(gl->ClearColor(0, 0, 0, 1));
		GLCheck(gl->Clear(GL_COLOR_BUFFER_BIT));

		{
			bool enableBlending = backend->enableBlending;
			RenderFilter minFilter = backend->minFilter;
			RenderFilter magFilter = backend->magFilter;

			backend->enableBlending = false;
			backend->minFilter = RenderFilter_Linear;
			backend->magFilter = RenderFilter_Linear;

			OpenGLRenderBackendDrawPrimitives(backend, GL_QUADS,
											  data->mainFrameBufferTexture,
											  vertices, ArrayLength(vertices));

			backend->enableBlending = enableBlending;
			backend->minFilter = minFilter;
			backend->magFilter = magFilter;
		}
	}
}
