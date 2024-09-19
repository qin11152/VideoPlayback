#include "OpenGLPreviewWidget.h"

#include <libyuv/libyuv.h>

void ConvertYUYV422ToYUV420(const uint8_t *yuyv, int width, int height,
							uint8_t *y_plane, uint8_t *u_plane, uint8_t *v_plane)
{
	int yuyv_stride = width * 2;
	int y_stride = width;
	int uv_stride = width / 2;

	// ʹ��libyuv����ת��,yuv422��yuv420
	libyuv::YUY2ToI420(yuyv, yuyv_stride, y_plane, y_stride, u_plane, uv_stride, v_plane, uv_stride, width, height);
}

void ConvertYUV422pToYUV420p(const uint8_t *src_yuv422p, int width, int height, uint8_t *dst_yuv420p)
{
	// ����YUV422pƽ�����ʼλ��
	// ����YUV422pƽ�����ʼλ��
	const uint8_t *src_y = src_yuv422p;
	const uint8_t *src_u = src_y + width * height;
	const uint8_t *src_v = src_u + (width * height / 2);

	// ����YUV420pƽ�����ʼλ��
	uint8_t *dst_y = dst_yuv420p;
	uint8_t *dst_u = dst_y + width * height;
	uint8_t *dst_v = dst_u + (width * height / 4);

	// ʹ��libyuv����ת��
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
		glBindTexture(GL_TEXTURE_2D, texs[0]);							  // �󶨲���
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // ��������������Ŵ���ˣ����Բ�ֵ GL_NEAREST(Ч�ʸߣ�Ч����)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth, m_uiHeight, 0, GL_RED, GL_UNSIGNED_BYTE, 0); // ���������Կ��ռ�

		//  U
		glBindTexture(GL_TEXTURE_2D, texs[1]);							  // �󶨲���
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // ��������������Ŵ���ˣ����Բ�ֵ
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth / 2, m_uiHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0); // ���������Կ��ռ�

		// �������� V
		glBindTexture(GL_TEXTURE_2D, texs[2]);							  // �󶨲���
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // ��������������Ŵ���ˣ����Բ�ֵ
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth / 2, m_uiHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0); // ���������Կ��ռ�
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
	initializeOpenGLFunctions(); // ��ʼ��opengl

	// program����shader�ű�(�����ƬԪ)
	program.addShaderFromSourceCode(QGLShader::Fragment, tString); // ƬԪ
	program.addShaderFromSourceCode(QGLShader::Vertex, vString);   // ����

	//// ���ö�������ı���
	program.bindAttributeLocation("vertexIn", A_VER);
	// ���ò�������
	program.bindAttributeLocation("textureIn", T_VER);
	program.link();
	program.bind();

	// ���ݶ���Ͳ�������
	// ����
	static const GLfloat ver[] = {
		-1.0f, -1.0f,
		1.0f, -1.0f,
		-1.0f, 1.0f,
		1.0f, 1.0f};
	// ����
	static const GLfloat tex[] = {
		0.0f, 1.0f,
		1.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f};
	/*
	attribute vec4 vertexIn;
	attribute vec2 textureIn;
	varying vec2 textureOut;*/
	glVertexAttribPointer(A_VER, 2, GL_FLOAT, 0, 0, ver); // ���� vertexIn
	glEnableVertexAttribArray(A_VER);					  // ���ö�������
	glVertexAttribPointer(T_VER, 2, GL_FLOAT, 0, 0, tex); // ����
	glEnableVertexAttribArray(T_VER);					  // ��Ч

	// ��shader��ȡ����
	unis[0] = program.uniformLocation("tex_y");
	unis[1] = program.uniformLocation("tex_u");
	unis[2] = program.uniformLocation("tex_v");

	// ��������
	glGenTextures(3, texs);
	// Y
	glBindTexture(GL_TEXTURE_2D, texs[0]);							  // �󶨲���
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // ��������������Ŵ���ˣ����Բ�ֵ GL_NEAREST(Ч�ʸߣ�Ч����)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth, m_uiHeight, 0, GL_RED, GL_UNSIGNED_BYTE, 0); // ���������Կ��ռ�

	//  U
	glBindTexture(GL_TEXTURE_2D, texs[1]);							  // �󶨲���
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // ��������������Ŵ���ˣ����Բ�ֵ
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth / 2, m_uiHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0); // ���������Կ��ռ�

	// �������� V
	glBindTexture(GL_TEXTURE_2D, texs[2]);							  // �󶨲���
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // ��������������Ŵ���ˣ����Բ�ֵ
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_uiWidth / 2, m_uiHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0); // ���������Կ��ռ�
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
	glActiveTexture(GL_TEXTURE0);		   // �����0��
	glBindTexture(GL_TEXTURE_2D, texs[0]); // 0��󶨵�y������
	// �޸Ĳ������ݣ������ڴ����ݣ�
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiWidth, m_uiHeight, GL_RED, GL_UNSIGNED_BYTE, m_YUV420Data.left(m_uiWidth * m_uiHeight).data());
	glUniform1i(unis[0], 0); // ��shader��uni��������  unis[0] = program.uniformLocation("tex_y")

	glActiveTexture(GL_TEXTURE0 + 1);	   // �����1��
	glBindTexture(GL_TEXTURE_2D, texs[1]); // 1��󶨵�U������
	// �޸Ĳ������ݣ������ڴ����ݣ�
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiWidth / 2, m_uiHeight / 2, GL_RED, GL_UNSIGNED_BYTE, m_YUV420Data.mid(m_uiWidth * m_uiHeight, m_uiWidth * m_uiHeight / 4).data());
	glUniform1i(unis[1], 1); // ��shader��uni��������     unis[1] = program.uniformLocation("tex_u");

	glActiveTexture(GL_TEXTURE0 + 2);	   // �����2��  V
	glBindTexture(GL_TEXTURE_2D, texs[2]); // 2��󶨵�v������
	// �޸Ĳ������ݣ������ڴ����ݣ�
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiWidth / 2, m_uiHeight / 2, GL_RED, GL_UNSIGNED_BYTE, m_YUV420Data.right(m_uiWidth * m_uiHeight / 4).data());
	glUniform1i(unis[2], 2); // ��shader��uni��������      unis[2] = program.uniformLocation("tex_v");

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); // ��ʼ����
}
