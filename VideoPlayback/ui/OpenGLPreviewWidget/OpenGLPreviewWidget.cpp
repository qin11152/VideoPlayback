#include "OpenGLPreviewWidget.h"

#include <libyuv/libyuv.h>

void ConvertYUYV422ToYUV420(const uint8_t *yuyv, int width, int height,
							uint8_t *y_plane, uint8_t *u_plane, uint8_t *v_plane)
{
	int yuyv_stride = width * 2;
	int y_stride = width;
	int uv_stride = width / 2;

	// 使用libyuv进行转换,yuv422到yuv420
	libyuv::YUY2ToI420(yuyv, yuyv_stride, y_plane, y_stride, u_plane, uv_stride, v_plane, uv_stride, width, height);
}

void ConvertYUV422pToYUV420p(const uint8_t *src_yuv422p, int width, int height, uint8_t *dst_yuv420p)
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
	int result = libyuv::I422ToI420(
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

// void OpenGLPreviewWidget::onSignalYUVData(QByteArray data, int width, int height)
void OpenGLPreviewWidget::onSignalYUVData(QByteArray data, const VideoCallbackInfo &videoInfo)
{
	if (m_uiWidth != videoInfo.width || m_uiHeight != videoInfo.height)
	{
		m_uiWidth = videoInfo.width;
		m_uiHeight = videoInfo.height;
		glBindTexture(GL_TEXTURE_2D, texs[0]);							  // 绑定材质
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 设置纹理参数，放大过滤，线性插值 GL_NEAREST(效率高，效果差)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth, m_uiHeight, 0, GL_RED, GL_UNSIGNED_BYTE, 0); // 创建材质显卡空间

		//  U
		glBindTexture(GL_TEXTURE_2D, texs[1]);							  // 绑定材质
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 设置纹理参数，放大过滤，线性插值
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth / 2, m_uiHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0); // 创建材质显卡空间

		// 创建材质 V
		glBindTexture(GL_TEXTURE_2D, texs[2]);							  // 绑定材质
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 设置纹理参数，放大过滤，线性插值
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth / 2, m_uiHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0); // 创建材质显卡空间
	}
	uint8_t* yuyv422 = new uint8_t[videoInfo.dataSize]{ 0 };
	memcpy(yuyv422, data.data(), data.size());
	uint8_t* yuv420 = new uint8_t[m_uiWidth * m_uiHeight * 3 / 2]{ 0 };
	//ConvertYUYV422ToYUV420(yuyv422, width, height, yuv420, yuv420 + width * height, yuv420 + width * height * 5 / 4);
	ConvertYUV422pToYUV420p(yuyv422, m_uiWidth, m_uiHeight, yuv420);
	m_YUV420Data = QByteArray((char*)yuv420, m_uiWidth * m_uiHeight * 3 / 2);
	//printf("convert time :%llu\n", std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
	delete[]yuv420;
	delete[]yuyv422;
	update();
}

void OpenGLPreviewWidget::initializeGL()
{
	initializeOpenGLFunctions(); // 初始化opengl

	// program加载shader脚本(顶点和片元)
	program.addShaderFromSourceCode(QGLShader::Fragment, tString); // 片元
	program.addShaderFromSourceCode(QGLShader::Vertex, vString);   // 顶点

	//// 设置顶点坐标的变量
	program.bindAttributeLocation("vertexIn", A_VER);
	// 设置材质坐标
	program.bindAttributeLocation("textureIn", T_VER);
	program.link();
	program.bind();

	// 传递顶点和材质坐标
	// 顶点
	static const GLfloat ver[] = {
		-1.0f, -1.0f,
		1.0f, -1.0f,
		-1.0f, 1.0f,
		1.0f, 1.0f};
	// 材质
	static const GLfloat tex[] = {
		0.0f, 1.0f,
		1.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f};
	/*
	attribute vec4 vertexIn;
	attribute vec2 textureIn;
	varying vec2 textureOut;*/
	glVertexAttribPointer(A_VER, 2, GL_FLOAT, 0, 0, ver); // 顶点 vertexIn
	glEnableVertexAttribArray(A_VER);					  // 启用顶点数组
	glVertexAttribPointer(T_VER, 2, GL_FLOAT, 0, 0, tex); // 材质
	glEnableVertexAttribArray(T_VER);					  // 生效

	// 从shader获取材质
	unis[0] = program.uniformLocation("tex_y");
	unis[1] = program.uniformLocation("tex_u");
	unis[2] = program.uniformLocation("tex_v");

	// 创建材质
	glGenTextures(3, texs);
	// Y
	glBindTexture(GL_TEXTURE_2D, texs[0]);							  // 绑定材质
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 设置纹理参数，放大过滤，线性插值 GL_NEAREST(效率高，效果差)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth, m_uiHeight, 0, GL_RED, GL_UNSIGNED_BYTE, 0); // 创建材质显卡空间

	//  U
	glBindTexture(GL_TEXTURE_2D, texs[1]);							  // 绑定材质
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 设置纹理参数，放大过滤，线性插值
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth / 2, m_uiHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0); // 创建材质显卡空间

	// 创建材质 V
	glBindTexture(GL_TEXTURE_2D, texs[2]);							  // 绑定材质
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 设置纹理参数，放大过滤，线性插值
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth / 2, m_uiHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0); // 创建材质显卡空间
}

void OpenGLPreviewWidget::resizeGL(int w, int h)
{
	glViewport(0, 0, w, h);
}

void OpenGLPreviewWidget::paintGL()
{
	if (m_YUV420Data.isEmpty())
	{
		return;
	}
	glActiveTexture(GL_TEXTURE0);		   // 激活第0层
	glBindTexture(GL_TEXTURE_2D, texs[0]); // 0层绑定到y材质中
	// 修改材质内容（复制内存内容）
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiWidth, m_uiHeight, GL_RED, GL_UNSIGNED_BYTE, m_YUV420Data.left(m_uiWidth * m_uiHeight).data());
	glUniform1i(unis[0], 0); // 与shader的uni变量关联  unis[0] = program.uniformLocation("tex_y")

	glActiveTexture(GL_TEXTURE0 + 1);	   // 激活第1层
	glBindTexture(GL_TEXTURE_2D, texs[1]); // 1层绑定到U材质中
	// 修改材质内容（复制内存内容）
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiWidth / 2, m_uiHeight / 2, GL_RED, GL_UNSIGNED_BYTE, m_YUV420Data.mid(m_uiWidth * m_uiHeight, m_uiWidth * m_uiHeight / 4).data());
	glUniform1i(unis[1], 1); // 与shader的uni变量关联     unis[1] = program.uniformLocation("tex_u");

	glActiveTexture(GL_TEXTURE0 + 2);	   // 激活第2层  V
	glBindTexture(GL_TEXTURE_2D, texs[2]); // 2层绑定到v材质中
	// 修改材质内容（复制内存内容）
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiWidth / 2, m_uiHeight / 2, GL_RED, GL_UNSIGNED_BYTE, m_YUV420Data.right(m_uiWidth * m_uiHeight / 4).data());
	glUniform1i(unis[2], 2); // 与shader的uni变量关联      unis[2] = program.uniformLocation("tex_v");

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); // 开始绘制
}
