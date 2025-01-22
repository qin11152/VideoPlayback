#include "OpenGLPreviewWidget.h"

#include <libyuv/libyuv.h>

void convertYUYV422ToYUV420(const uint8_t *yuyv, int width, int height,
							uint8_t *y_plane, uint8_t *u_plane, uint8_t *v_plane)
{
	int yuyv_stride = width * 2;
	int y_stride = width;
	int uv_stride = width / 2;

	// 使用libyuv进行转换,yuyv422到yuv420
	libyuv::YUY2ToI420(yuyv, yuyv_stride, y_plane, y_stride, u_plane, uv_stride, v_plane, uv_stride, width, height);
}

void convertUYVY422ToYUV420(const uint8_t *uyvy, int width, int height, uint8_t *y_plane, uint8_t *u_plane, uint8_t *v_plane)
{
	int uyvy_stride = width * 2;
	int y_stride = width;
	int uv_stride = width / 2;

	// 使用libyuv进行转换,uyvy422到yuv420
	libyuv::UYVYToI420(uyvy, uyvy_stride, y_plane, y_stride, u_plane, uv_stride, v_plane, uv_stride, width, height);
}

void convertYUV422pToYUV420p(const uint8_t *src_yuv422p, int width, int height, uint8_t *dst_yuv420p)
{
	// 计算YUV422p平面的起始位置
	// 计算YUV422p平面的起始位置
	const uint8_t *src_y = src_yuv422p;
	const uint8_t *src_u = src_y + width * height;
	const uint8_t *src_v = src_u + (width * height / 2);

	// 计算YUV420p平面的起始位置
	uint8_t *dst_y = dst_yuv420p;
	uint8_t *dst_u = dst_y + width * height;
	uint8_t *dst_v = dst_u + (width * height / 4);

	// 使用libyuv进行转换
	libyuv::I422ToI420(
		src_y, width,
		src_u, width / 2,
		src_v, width / 2,
		dst_y, width,
		dst_u, width / 2,
		dst_v, width / 2,
		width, height);
}

OpenGLPreviewWidget::OpenGLPreviewWidget(QWidget *parent)
	: QOpenGLWidget(parent)
{
}

OpenGLPreviewWidget::~OpenGLPreviewWidget()
{
}

void OpenGLPreviewWidget::onSignalYUVData(QByteArray data, const VideoInfo &videoInfo)
{
	if (AV_PIX_FMT_YUV420P != videoInfo.videoFormat && AV_PIX_FMT_UYVY422 != videoInfo.videoFormat)
	{
		return;
	}
	setVideoFormat(videoInfo.videoFormat);
	if (m_uiWidth != videoInfo.width || m_uiHeight != videoInfo.height)
	{
		m_uiWidth = videoInfo.width;
		m_uiHeight = videoInfo.height;
		makeCurrent();
		if (m_format == AV_PIX_FMT_YUV420P)
		{
			glBindTexture(GL_TEXTURE_2D, yuv420Textures[0]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth, m_uiHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

			glBindTexture(GL_TEXTURE_2D, yuv420Textures[1]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth / 2, m_uiHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

			glBindTexture(GL_TEXTURE_2D, yuv420Textures[2]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth / 2, m_uiHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
		}
		else // VideoFormat::UYVY
		{
			glBindTexture(GL_TEXTURE_2D, uyvyTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_uiWidth / 2, m_uiHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		}
		doneCurrent();
		updateVertices();
	}
	m_videoData = data;
	update();
}

void OpenGLPreviewWidget::initializeGL()
{
	initializeOpenGLFunctions();

	// 初始化两种格式的程序
	initYUV420Program();
	initUYVYProgram();

	// 设置默认程序
	currentProgram = (m_format == AV_PIX_FMT_YUV420P) ? &yuv420Program : &uyvyProgram;

	// 设置顶点数据（两种格式共用）
	currentProgram->bind();
	GLuint verLocation = currentProgram->attributeLocation("vertexIn");
	GLuint texLocation = currentProgram->attributeLocation("textureIn");

	glVertexAttribPointer(verLocation, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), vertices);
	glEnableVertexAttribArray(verLocation);
	glVertexAttribPointer(texLocation, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), vertices + 3);
	glEnableVertexAttribArray(texLocation);

}

void OpenGLPreviewWidget::resizeGL(int w, int h)
{
	glViewport(0, 0, w, h);
	updateVertices();
}

void OpenGLPreviewWidget::paintGL()
{
	//if (m_videoData.isEmpty() || !currentProgram)
	//	return;

	//currentProgram->bind();
	//updateTextures();
	//glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	if (m_videoData.isEmpty() || !currentProgram)
	{
		return;
	}

	currentProgram->bind();
	if (m_format == AV_PIX_FMT_YUV420P)
	{
		updateYUV420Textures();
	}
	else
	{
		updateUYVYTextures();
	}

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void OpenGLPreviewWidget::initYUV420Program()
{
	yuv420Program.addShaderFromSourceCode(QGLShader::Vertex, vString);
	yuv420Program.addShaderFromSourceCode(QGLShader::Fragment, yuv420String);
	yuv420Program.link();

	yuv420Program.bind();
	// 获取YUV纹理uniform位置
	yuv420Uniforms[0] = yuv420Program.uniformLocation("tex_y");
	yuv420Uniforms[1] = yuv420Program.uniformLocation("tex_u");
	yuv420Uniforms[2] = yuv420Program.uniformLocation("tex_v");

	// 创建YUV纹理
	glGenTextures(3, yuv420Textures);

	// 初始化Y纹理
	glBindTexture(GL_TEXTURE_2D, yuv420Textures[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth, m_uiHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

	// 初始化U纹理
	glBindTexture(GL_TEXTURE_2D, yuv420Textures[1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth / 2, m_uiHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

	// 初始化V纹理
	glBindTexture(GL_TEXTURE_2D, yuv420Textures[2]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth / 2, m_uiHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
}

void OpenGLPreviewWidget::initUYVYProgram()
{
	uyvyProgram.addShaderFromSourceCode(QGLShader::Vertex, vString);
	uyvyProgram.addShaderFromSourceCode(QGLShader::Fragment, uyvyString);
	uyvyProgram.link();

	uyvyProgram.bind();
	// 获取UYVY相关uniform位置
	uyvyWidthUniform = uyvyProgram.uniformLocation("texWidth");
	GLuint uyvyUniform = uyvyProgram.uniformLocation("tex_uyvy");

	// 创建UYVY纹理
	glGenTextures(1, &uyvyTexture);
	glBindTexture(GL_TEXTURE_2D, uyvyTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_uiWidth / 2, m_uiHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glUniform1i(uyvyUniform, 0);
}

void OpenGLPreviewWidget::updateTextures()
{
	if (m_format == AV_PIX_FMT_YUV420P)
	{
		// 更新Y纹理
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, yuv420Textures[0]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiWidth, m_uiHeight, GL_RED, GL_UNSIGNED_BYTE,
			m_videoData.left(m_uiWidth * m_uiHeight).data());
		glUniform1i(yuv420Uniforms[0], 0);

		// 更新U纹理
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, yuv420Textures[1]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiWidth / 2, m_uiHeight / 2, GL_RED, GL_UNSIGNED_BYTE,
			m_videoData.mid(m_uiWidth * m_uiHeight, m_uiWidth * m_uiHeight / 4).data());
		glUniform1i(yuv420Uniforms[1], 1);

		// 更新V纹理
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, yuv420Textures[2]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiWidth / 2, m_uiHeight / 2, GL_RED, GL_UNSIGNED_BYTE,
			m_videoData.right(m_uiWidth * m_uiHeight / 4).data());
		glUniform1i(yuv420Uniforms[2], 2);
	}
	else // VideoFormat::UYVY
	{
		glUniform1f(uyvyWidthUniform, m_uiWidth);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, uyvyTexture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiWidth / 2, m_uiHeight,
			GL_RGBA, GL_UNSIGNED_BYTE, m_videoData.data());
	}
}

void OpenGLPreviewWidget::updateVertices()
{
	float widgetAspect = static_cast<float>(width()) / height();
	float videoAspect = static_cast<float>(m_uiWidth) / m_uiHeight;

	float scaleX = 1.0f;
	float scaleY = 1.0f;

	if (videoAspect > widgetAspect) {
		scaleY = widgetAspect / videoAspect;
	}
	else {
		scaleX = videoAspect / widgetAspect;
	}

	float newVertices[] = {
		-scaleX, -scaleY, 0.0f,  0.0f, 1.0f,  // 左下角
		-scaleX,  scaleY, 0.0f,  0.0f, 0.0f,  // 左上角
		 scaleX,  scaleY, 0.0f,  1.0f, 0.0f,  // 右上角
		 scaleX, -scaleY, 0.0f,  1.0f, 1.0f   // 右下角
	};

	std::memcpy(vertices, newVertices, sizeof(newVertices));

	if (m_format == AV_PIX_FMT_YUV420P) {
		yuv420Program.bind();
		GLuint verLocation = yuv420Program.attributeLocation("vertexIn");
		GLuint texLocation = yuv420Program.attributeLocation("textureIn");

		glVertexAttribPointer(verLocation, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), vertices);
		glEnableVertexAttribArray(verLocation);
		glVertexAttribPointer(texLocation, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), vertices + 3);
		glEnableVertexAttribArray(texLocation);
	}
	else if (m_format == AV_PIX_FMT_UYVY422) {
		uyvyProgram.bind();
		GLuint verLocation = uyvyProgram.attributeLocation("vertexIn");
		GLuint texLocation = uyvyProgram.attributeLocation("textureIn");

		glVertexAttribPointer(verLocation, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), vertices);
		glEnableVertexAttribArray(verLocation);
		glVertexAttribPointer(texLocation, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), vertices + 3);
		glEnableVertexAttribArray(texLocation);
	}

}

void OpenGLPreviewWidget::updateYUV420Textures()
{
	// 更新Y纹理
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, yuv420Textures[0]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiWidth, m_uiHeight, GL_RED, GL_UNSIGNED_BYTE,
		m_videoData.left(m_uiWidth * m_uiHeight).data());
	glUniform1i(yuv420Uniforms[0], 0);

	// 更新U纹理
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, yuv420Textures[1]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiWidth / 2, m_uiHeight / 2, GL_RED, GL_UNSIGNED_BYTE,
		m_videoData.mid(m_uiWidth * m_uiHeight, m_uiWidth * m_uiHeight / 4).data());
	glUniform1i(yuv420Uniforms[1], 1);

	// 更新V纹理
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, yuv420Textures[2]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiWidth / 2, m_uiHeight / 2, GL_RED, GL_UNSIGNED_BYTE,
		m_videoData.right(m_uiWidth * m_uiHeight / 4).data());
	glUniform1i(yuv420Uniforms[2], 2);
}

void OpenGLPreviewWidget::updateUYVYTextures()
{
	uyvyProgram.bind();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, uyvyTexture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiWidth / 2, m_uiHeight, GL_RGBA, GL_UNSIGNED_BYTE,
		m_videoData.data());
	glUniform1i(uyvyWidthUniform, 0);
}

void OpenGLPreviewWidget::setVideoFormat(AVPixelFormat format)
{
	if (m_format != format)
	{
		m_format = format;
		currentProgram = (format == AV_PIX_FMT_YUV420P) ? &yuv420Program : &uyvyProgram;

		// 如果已经有数据，需要重新创建纹理
		if (m_uiWidth > 0 && m_uiHeight > 0)
		{
			makeCurrent();
			if (format == AV_PIX_FMT_YUV420P)
			{
				glBindTexture(GL_TEXTURE_2D, yuv420Textures[0]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth, m_uiHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

				glBindTexture(GL_TEXTURE_2D, yuv420Textures[1]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth / 2, m_uiHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

				glBindTexture(GL_TEXTURE_2D, yuv420Textures[2]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth / 2, m_uiHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
			}
			else
			{
				glBindTexture(GL_TEXTURE_2D, uyvyTexture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_uiWidth / 2, m_uiHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			}
			doneCurrent();
		}
		update();
	}
}
