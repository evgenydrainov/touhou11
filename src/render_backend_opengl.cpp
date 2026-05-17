#include "render_backend_opengl.h"
#include "game_assets.h"
#include "stb_sprintf.h"
#include "debug.h"

static char g_vertexShaderPreamble[] =
R"(#ifdef GL_ES
	precision mediump float;
	#define IN attribute
	#define OUT varying
#else
	#define IN in
	#define OUT out
#endif
)";

static char g_fragmentShaderPreamble[] =
R"(#ifdef GL_ES
	precision mediump float;
	#define IN varying
	#define FRAG_COLOR gl_FragColor
	#define TEXTURE texture2D
#else
	out vec4 out_fragcolor;
	#define IN in
	#define TEXTURE texture
	#define FRAG_COLOR out_fragcolor
#endif
)";

static char g_defaultShaderVert[] =
R"(IN vec2 aPosition;
IN vec2 aTexCoord;
IN vec4 aColor;

OUT vec2 v_texCoord;
OUT vec4 v_color;

uniform mat4 u_projection;

void main()
{
	gl_Position = u_projection * vec4(aPosition.x, aPosition.y, 0.0, 1.0);
	v_texCoord = aTexCoord;
	v_color = aColor;
}
)";

static char g_defaultShaderVert3D[] =
R"(IN vec3 aPosition;
IN vec2 aTexCoord;
IN vec4 aColor;

OUT vec2 v_texCoord;
OUT vec4 v_color;

uniform mat4 u_modelView;
uniform mat4 u_projection;

void main()
{
	gl_Position = (u_projection * u_modelView) * vec4(aPosition.x, aPosition.y, aPosition.z, 1.0);
	v_texCoord = aTexCoord;
	v_color = aColor;
}
)";

static char g_defaultShaderFrag[] =
R"(IN vec2 v_texCoord;
IN vec4 v_color;
uniform sampler2D u_texture;

void main()
{
	FRAG_COLOR = TEXTURE(u_texture, v_texCoord) * v_color;
}
)";

static char g_fogShaderVert3D[] =
R"(IN vec3 aPosition;
IN vec2 aTexCoord;
IN vec4 aColor;

OUT vec2 v_texCoord;
OUT vec4 v_color;
OUT float v_fogDistance;

uniform mat4 u_modelView;
uniform mat4 u_projection;

void main()
{
	vec4 viewPosition = u_modelView * vec4(aPosition.x, aPosition.y, aPosition.z, 1.0);

	gl_Position = u_projection * viewPosition;

	v_texCoord = aTexCoord;
	v_color = aColor;
	v_fogDistance = length(viewPosition.xyz);
}
)";

static char g_fogShaderFrag3D[] =
R"(IN vec2 v_texCoord;
IN vec4 v_color;
IN float v_fogDistance;

uniform sampler2D u_texture;
uniform vec4 u_fogColor;
uniform float u_fogNear;
uniform float u_fogFar;

void main()
{
	vec4 color = TEXTURE(u_texture, v_texCoord);

	float fogAmount = smoothstep(u_fogNear, u_fogFar, v_fogDistance);
	color.rgb = mix(color.rgb, u_fogColor.rgb, fogAmount);

	FRAG_COLOR = color * v_color;
}
)";

static const char *
GetGLSLVersionString(OpenGLContextVersion gl_version)
{
	switch (gl_version)
	{
		case OpenGLContextVersion_3_3_Core: return "#version 330 core\n";
		case OpenGLContextVersion_2_0_ES: return "#version 100\n";
		case OpenGLContextVersion_1_1: return "";
	}

	Assert(false);
	return "";
}

static const char *
GetGLSLShaderPreamble(u32 shaderType)
{
	switch (shaderType)
	{
		case GL_VERTEX_SHADER: return g_vertexShaderPreamble;
		case GL_FRAGMENT_SHADER: return g_fragmentShaderPreamble;
	}

	Assert(false);
	return "";
}

static u32
CompileShader(RenderBackendOpenGLData *data,
			  GLenum type,
			  const char *source)
{
	OpenGLFunctions *gl = data->gl;

	u32 shader = gl->CreateShader(type);

	const char *versionString = GetGLSLVersionString(data->gl_version);
	const char *preamble = GetGLSLShaderPreamble(type);

	const char *sources[] = {versionString, preamble, source};
	gl->ShaderSource(shader, ArrayLength(sources), sources, nullptr);

	gl->CompileShader(shader);

	int success;
	gl->GetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		char message[512];
		gl->GetShaderInfoLog(shader, sizeof(message), nullptr, message);

		LogError("Shader Compile Error:\n%s", message);
	}

	return shader;
}

static u32
LinkProgram(RenderBackendOpenGLData *data,
			u32 vertexShader,
			u32 fragmentShader)
{
	OpenGLFunctions *gl = data->gl;

	u32 program = gl->CreateProgram();

	gl->AttachShader(program, vertexShader);
	gl->AttachShader(program, fragmentShader);

	gl->BindAttribLocation(program, 0, "aPosition");
	gl->BindAttribLocation(program, 1, "aTexCoord");
	gl->BindAttribLocation(program, 2, "aColor");

	gl->LinkProgram(program);

	int success;
	gl->GetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success)
	{
		char message[512];
		gl->GetProgramInfoLog(program, sizeof(message), nullptr, message);

		LogError("Shader Link Error:\n%s", message);
	}

	return program;
}

static u32
LoadShaderFromStrings(RenderBackendOpenGLData *data,
					  const char *vertexSrc,
					  const char *fragmentSrc)
{
	OpenGLFunctions *gl = data->gl;

	u32 vertexShader = CompileShader(data, GL_VERTEX_SHADER, vertexSrc);
	u32 fragmentShader = CompileShader(data, GL_FRAGMENT_SHADER, fragmentSrc);

	u32 program = LinkProgram(data, vertexShader, fragmentShader);

	gl->DeleteShader(vertexShader);
	gl->DeleteShader(fragmentShader);

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

	backend->DrawQuads = OpenGLRenderBackendDrawQuads;
	backend->DrawTriangles3D = OpenGLRenderBackendDrawTriangles3D;
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

		gl->GenBuffers(1, &data->quadIndexBuffer);
		gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, data->quadIndexBuffer);
		gl->BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0])*data->numQuadIndices, indices, GL_STATIC_DRAW);

		gl->GenBuffers(1, &data->vertexBuffer);
		gl->BindBuffer(GL_ARRAY_BUFFER, data->vertexBuffer);
		gl->BufferData(GL_ARRAY_BUFFER, data->vertexBufferSize, nullptr, GL_DYNAMIC_DRAW);

		data->shaders[ShaderIndex_Normal] = LoadShaderFromStrings(data, g_defaultShaderVert, g_defaultShaderFrag);
		data->shaders[ShaderIndex_Normal3D] = LoadShaderFromStrings(data, g_defaultShaderVert3D, g_defaultShaderFrag);
		data->shaders[ShaderIndex_Fog3D] = LoadShaderFromStrings(data, g_fogShaderVert3D, g_fogShaderFrag3D);

		gl->GenTextures(1, &data->mainFrameBufferTexture);
		gl->BindTexture(GL_TEXTURE_2D, data->mainFrameBufferTexture);

		gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mainFrameBufferWidth, mainFrameBufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

		gl->GenFramebuffers(1, &data->mainFrameBuffer);
		gl->BindFramebuffer(GL_FRAMEBUFFER, data->mainFrameBuffer);
		gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, data->mainFrameBufferTexture, 0);
	}

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
		gl->EnableClientState(GL_VERTEX_ARRAY);
		gl->VertexPointer(2, GL_FLOAT, sizeof(RenderVertex2D), (u8*)vertices + OffsetOf(RenderVertex2D, x));

		gl->EnableClientState(GL_TEXTURE_COORD_ARRAY);
		gl->TexCoordPointer(2, GL_FLOAT, sizeof(RenderVertex2D), (u8*)vertices + OffsetOf(RenderVertex2D, u));

		gl->EnableClientState(GL_COLOR_ARRAY);
		gl->ColorPointer(4, GL_UNSIGNED_BYTE, sizeof(RenderVertex2D), (u8*)vertices + OffsetOf(RenderVertex2D, color));
	}
	else
	{
		gl->VertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(RenderVertex2D), (void*)OffsetOf(RenderVertex2D, x));
		gl->EnableVertexAttribArray(0);

		gl->VertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(RenderVertex2D), (void*)OffsetOf(RenderVertex2D, u));
		gl->EnableVertexAttribArray(1);

		gl->VertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, true, sizeof(RenderVertex2D), (void*)OffsetOf(RenderVertex2D, color));
		gl->EnableVertexAttribArray(2);
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
		gl->EnableClientState(GL_VERTEX_ARRAY);
		gl->VertexPointer(3, GL_FLOAT, sizeof(RenderVertex3D), (u8*)vertices + OffsetOf(RenderVertex3D, x));

		gl->EnableClientState(GL_TEXTURE_COORD_ARRAY);
		gl->TexCoordPointer(2, GL_FLOAT, sizeof(RenderVertex3D), (u8*)vertices + OffsetOf(RenderVertex3D, u));

		gl->EnableClientState(GL_COLOR_ARRAY);
		gl->ColorPointer(4, GL_UNSIGNED_BYTE, sizeof(RenderVertex3D), (u8*)vertices + OffsetOf(RenderVertex3D, color));
	}
	else
	{
		gl->VertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(RenderVertex3D), (void*)OffsetOf(RenderVertex3D, x));
		gl->EnableVertexAttribArray(0);

		gl->VertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(RenderVertex3D), (void*)OffsetOf(RenderVertex3D, u));
		gl->EnableVertexAttribArray(1);

		gl->VertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, true, sizeof(RenderVertex3D), (void*)OffsetOf(RenderVertex3D, color));
		gl->EnableVertexAttribArray(2);
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

	usize vertexBufferSize = sizeof(vertices[0])*numVertices;
	if (vertexBufferSize <= data->vertexBufferSize)
	{
		gl->BindTexture(GL_TEXTURE_2D, textureID);
		gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GetGLFilter(backend->minFilter));
		gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GetGLFilter(backend->magFilter));

		if (backend->enableBlending)
		{
			gl->Enable(GL_BLEND);
			gl->BlendFunc(GetGLBlendFactor(backend->srcBlendFactor), GetGLBlendFactor(backend->destBlendFactor));
		}
		else
		{
			gl->Disable(GL_BLEND);
		}

		gl->Disable(GL_CULL_FACE);

		if (data->gl_version == OpenGLContextVersion_1_1)
		{
			gl->MatrixMode(GL_MODELVIEW);
			gl->LoadMatrixf(&backend->modelView.e[0]);

			gl->MatrixMode(GL_PROJECTION);
			gl->LoadMatrixf(&backend->projection.e[0]);

			gl->Enable(GL_TEXTURE_2D);

			OpenGLRenderBackendSetVertexLayout(backend, vertices);

			gl->DrawArrays(primitiveType, 0, numVertices);

			backend->currNumDrawCalls++;
		}
		else
		{
			gl->BindBuffer(GL_ARRAY_BUFFER, data->vertexBuffer);
			gl->BufferSubData(GL_ARRAY_BUFFER, 0, vertexBufferSize, vertices);

			gl->UseProgram(data->shaders[backend->shaderIndex]);

			{
				int location = gl->GetUniformLocation(data->shaders[backend->shaderIndex], "u_modelView");
				if (location != -1)
				{
					gl->UniformMatrix4fv(location, 1, false, &backend->modelView.e[0]);
				}
			}

			{
				int location = gl->GetUniformLocation(data->shaders[backend->shaderIndex], "u_projection");
				if (location != -1)
				{
					gl->UniformMatrix4fv(location, 1, false, &backend->projection.e[0]);
				}
			}

			OpenGLRenderBackendSetVertexLayout(backend, vertices);

			if (primitiveType == GL_QUADS)
			{
				int numQuadIndices = numVertices/4*6;

				if (numQuadIndices <= data->numQuadIndices)
				{
					gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, data->quadIndexBuffer);
					gl->DrawElements(GL_TRIANGLES, numQuadIndices, GL_UNSIGNED_SHORT, nullptr);

					backend->currNumDrawCalls++;
				}
				else
				{
					LogWarn("numQuadIndices is too big");
				}
			}
			else
			{
				gl->DrawArrays(primitiveType, 0, numVertices);

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

	OpenGLRenderBackendDrawPrimitives(backend, GL_TRIANGLES, textureID, vertices, num_vertices);
}

RENDER_BACKEND_UPLOAD_TEXTURE_ASSET(OpenGLRenderBackendUploadTextureAsset)
{
	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;
	OpenGLFunctions *gl = data->gl;

	u32 id;
	gl->GenTextures(1, &id);
	gl->BindTexture(GL_TEXTURE_2D, id);
	gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT /*GL_CLAMP_TO_EDGE*/);
	gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT /*GL_CLAMP_TO_EDGE*/);
	gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture->width, texture->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture->pixels);

	if (texture->wantMipMap)
	{
		if (gl->GenerateMipmap)
		{
			gl->GenerateMipmap(GL_TEXTURE_2D);
		}
	}

	texture->handle = (void *)(usize)id;

	return true;
}

RENDER_BACKEND_CLEAR(OpenGLRenderBackendClear)
{
	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;
	OpenGLFunctions *gl = data->gl;

	gl->ClearColor(r, g, b, a);
	gl->Clear(GL_COLOR_BUFFER_BIT);
}

RENDER_BACKEND_SET_VIEWPORT(OpenGLRenderBackendSetViewport)
{
	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;
	OpenGLFunctions *gl = data->gl;

	gl->Viewport(x, backend->targetHeight - height - y, width, height);
}

RENDER_BACKEND_SET_UNIFORM(OpenGLRenderBackendSetUniform)
{
	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;
	OpenGLFunctions *gl = data->gl;

	int location = gl->GetUniformLocation(data->shaders[backend->shaderIndex], name);
	if (location != -1)
	{
		gl->UseProgram(data->shaders[backend->shaderIndex]);

		switch (type)
		{
			case ShaderUniformType_F32:
			{
				gl->Uniform1fv(location, 1, (f32 *)value);
			} break;

			case ShaderUniformType_Vec4:
			{
				gl->Uniform4fv(location, 1, (f32 *)value);
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

	gl->BindFramebuffer(GL_FRAMEBUFFER, data->mainFrameBuffer);

	backend->targetWidth = data->mainFrameBufferWidth;
	backend->targetHeight = data->mainFrameBufferHeight;

	backend->numDrawCalls = backend->currNumDrawCalls;
	backend->currNumDrawCalls = 0;

	return true;
}

void
OpenGLRenderBackendPresent(RenderBackend *backend)
{
	TIMED_FUNCTION();

	RenderBackendOpenGLData *data = (RenderBackendOpenGLData *)backend->userdata;
	OpenGLFunctions *gl = data->gl;

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

	gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
	gl->Viewport(0, 0, backend->windowWidth, backend->windowHeight);

	gl->ClearColor(0, 0, 0, 1);
	gl->Clear(GL_COLOR_BUFFER_BIT);

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
