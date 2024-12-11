#pragma once

#include "CommonDef.h"

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QGLShaderProgram>

// 自动加双引号
#define GET_STR(x) #x
#define A_VER 3
#define T_VER 4

class OpenGLPreviewWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
	Q_OBJECT

public:
	OpenGLPreviewWidget(QWidget* parent = nullptr);
	~OpenGLPreviewWidget();

public slots:
	void onSignalYUVData(QByteArray data, const VideoInfo& videoInfo);

protected:
	void initializeGL() override;
	void resizeGL(int w, int h)override;
	void paintGL() override;

private:
	void initYUV420Program();  // 初始化YUV420相关资源
	void initUYVYProgram();    // 初始化UYVY相关资源
	void updateTextures();     // 更新纹理数据

	void setVideoFormat(AVPixelFormat format);

private:
	QGLShaderProgram yuv420Program;  // YUV420着色器程序
	QGLShaderProgram uyvyProgram;    // UYVY着色器程序
	QGLShaderProgram* currentProgram{ nullptr }; // 当前使用的程序

	// YUV420相关
	GLuint yuv420Uniforms[3] = { 0 };  // shader中的yuv变量地址
	GLuint yuv420Textures[3] = { 0 };  // YUV纹理

	// UYVY相关
	GLuint uyvyTexture = 0;          // UYVY纹理
	GLuint uyvyWidthUniform = 0;     // 纹理宽度uniform

	AVPixelFormat m_format{ AV_PIX_FMT_YUV420P }; // 视频格式

	QByteArray m_videoData;
	uint32_t m_uiWidth{ 0 };
	uint32_t m_uiHeight{ 0 };

	// 顶点着色器代码
	const char* vString = GET_STR(
		attribute vec4 vertexIn;
	attribute vec2 textureIn;
	varying vec2 textureOut;
	void main(void)
	{
		gl_Position = vertexIn;
		textureOut = textureIn;
	}
		);

	// YUV420P片元着色器
	const char* yuv420String = GET_STR(
		varying vec2 textureOut;
	uniform sampler2D tex_y;
	uniform sampler2D tex_u;
	uniform sampler2D tex_v;
	void main(void)
	{
		vec3 yuv;
		vec3 rgb;
		yuv.x = texture2D(tex_y, textureOut).r;
		yuv.y = texture2D(tex_u, textureOut).r - 0.5;
		yuv.z = texture2D(tex_v, textureOut).r - 0.5;
		rgb = yuv * mat3(1.0, 0.0, 1.28033,
			1.0, -0.21482, -0.38059,
			1.0, 2.12798, 0.0);
		gl_FragColor = vec4(rgb, 1.0);
	}
		);

	// UYVY片元着色器
	const char* uyvyString = GET_STR(
		varying vec2 textureOut;
	uniform sampler2D tex_uyvy;
	uniform float texWidth;
	void main(void)
	{
		vec3 rgb;
		float texX = textureOut.x;
		float isOdd = floor(mod(texX * texWidth, 2.0));
		vec2 texPos = vec2(texX, textureOut.y);
		vec4 uyvy = texture2D(tex_uyvy, texPos);
		float y = isOdd > 0.0 ? uyvy.a : uyvy.g;
		float u = uyvy.r - 0.5;
		float v = uyvy.b - 0.5;
		rgb = vec3(y, y, y) + vec3(1.403 * v,
			-0.344 * u - 0.714 * v,
			1.770 * u);
		gl_FragColor = vec4(rgb, 1.0);
	}
		);
};