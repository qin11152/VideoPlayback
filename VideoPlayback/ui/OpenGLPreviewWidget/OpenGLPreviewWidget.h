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
	QGLShaderProgram program;    // shader程序
	GLuint unis[3] = { 0 };   // shader中的yuv变量地址
	GLuint texs[3] = { 0 };   // opengl的texture地址
	QByteArray m_YUYV422Data;
	QByteArray m_YUV420Data;
	uint32_t m_uiWidth{ 0 };
	uint32_t m_uiHeight{ 0 };

	// 顶点shader
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

	// 片元shader
	const char* tString = GET_STR(
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

};
