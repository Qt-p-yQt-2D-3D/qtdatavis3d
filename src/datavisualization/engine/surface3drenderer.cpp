/****************************************************************************
**
** Copyright (C) 2013 Digia Plc
** All rights reserved.
** For any questions to Digia, please use contact form at http://qt.digia.com
**
** This file is part of the QtDataVisualization module.
**
** Licensees holding valid Qt Enterprise licenses may use this file in
** accordance with the Qt Enterprise License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.
**
** If you have questions regarding the use of this file, please use
** contact form at http://qt.digia.com
**
****************************************************************************/

#include "surface3dcontroller_p.h"
#include "surface3drenderer_p.h"
#include "q3dcamera.h"
#include "q3dcamera_p.h"
#include "shaderhelper_p.h"
#include "objecthelper_p.h"
#include "surfaceobject_p.h"
#include "texturehelper_p.h"
#include "selectionpointer_p.h"
#include "theme_p.h"
#include "utils_p.h"
#include "drawer_p.h"
#include "q3dlight.h"

#include <QMatrix4x4>
#include <QMouseEvent>
#include <qmath.h>

#include <QLinearGradient>
#include <QPainter>

#include <QDebug>

static const int ID_TO_RGBA_MASK = 0xff;

QT_DATAVISUALIZATION_BEGIN_NAMESPACE

//#define SHOW_DEPTH_TEXTURE_SCENE

// TODO Uniform scaling is broken on surface
//#define USE_UNIFORM_SCALING // Scale x and z uniformly, or based on autoscaled values

const GLfloat aspectRatio = 2.0f; // Forced ratio of x and z to y. Dynamic will make it look odd.
const GLfloat backgroundMargin = 1.1f; // Margin for background (1.1f = make it 10% larger to avoid items being drawn inside background)
const GLfloat labelMargin = 0.05f;
const GLfloat backgroundBottom = 1.0f;
const GLfloat gridLineWidth = 0.005f;
const GLfloat surfaceGridYOffsetValue = 0.001f;
// The second offset to opposite direction is double because same matrix is translated twice
const GLfloat surfaceGridYOffset[2] = {-surfaceGridYOffsetValue, 2.0f * surfaceGridYOffsetValue};

Surface3DRenderer::Surface3DRenderer(Surface3DController *controller)
    : Abstract3DRenderer(controller),
      m_controller(controller),
      m_labelTransparency(QDataVis::TransparencyFromTheme),
      m_font(QFont(QStringLiteral("Arial"))),
      m_isGridEnabled(true),
      m_segmentYCount(0),
      m_segmentYStep(0.0f),
      m_shader(0),
      m_depthShader(0),
      m_backgroundShader(0),
      m_surfaceShader(0),
      m_surfaceGridShader(0),
      m_selectionShader(0),
      m_labelShader(0),
      m_heightNormalizer(0.0f),
      m_scaleFactor(0.0f),
      m_scaleX(0.0f),
      m_scaleZ(0.0f),
      m_scaleXWithBackground(0.0f),
      m_scaleZWithBackground(0.0f),
      m_surfaceScaleX(0.0f),
      m_surfaceScaleZ(0.0f),
      m_surfaceOffsetX(0.0f),
      m_surfaceOffsetZ(0.0f),
      m_minVisibleColumnValue(0.0f),
      m_maxVisibleColumnValue(0.0f),
      m_minVisibleRowValue(0.0f),
      m_maxVisibleRowValue(0.0f),
      m_dataStepX(0.0f),
      m_dataStepZ(0.0f),
      m_backgroundObj(0),
      m_gridLineObj(0),
      m_labelObj(0),
      m_surfaceObj(0),
      m_depthTexture(0),
      m_depthFrameBuffer(0),
      m_selectionFrameBuffer(0),
      m_selectionDepthBuffer(0),
      m_gradientTexture(0),
      m_selectionTexture(0),
      m_selectionResultTexture(0),
      m_shadowQualityToShader(33.3f),
      m_querySelection(false),
      m_flatSupported(true),
      m_selectionPointer(0),
      m_selectionActive(false),
      m_xFlipped(false),
      m_zFlipped(false),
      m_yFlipped(false),
      m_sampleSpace(QRect(0, 0, 0, 0)),
      m_shadowQualityMultiplier(3),
      m_hasHeightAdjustmentChanged(true)
{
    // Listen to changes in the controller
    QObject::connect(m_controller, &Surface3DController::leftMousePressed, this,
                     &Surface3DRenderer::requestSelectionAtPoint); // TODO: Possible temp

#if !defined(QT_OPENGL_ES_2)
    // Check if flat feature is supported
    ShaderHelper tester(this, QStringLiteral(":/shaders/vertexSurfaceFlat"),
                        QStringLiteral(":/shaders/fragmentSurfaceFlat"));
    if (!tester.testCompile()) {
        m_flatSupported = false;
        m_controller->setSmoothSurface(true);
    }
#endif

    m_cachedSmoothSurface =  m_controller->smoothSurface();
    updateSurfaceGridStatus(m_controller->surfaceGrid());

    // Shadows are disabled for Q3DSurface in Tech Preview
    updateShadowQuality(QDataVis::ShadowNone);

    initializeOpenGLFunctions();
    initializeOpenGL();
}

Surface3DRenderer::~Surface3DRenderer()
{
    m_textureHelper->glDeleteFramebuffers(1, &m_depthFrameBuffer);
    m_textureHelper->glDeleteRenderbuffers(1, &m_selectionDepthBuffer);
    m_textureHelper->glDeleteFramebuffers(1, &m_selectionFrameBuffer);

    m_textureHelper->deleteTexture(&m_depthTexture);
    m_textureHelper->deleteTexture(&m_gradientTexture);
    m_textureHelper->deleteTexture(&m_selectionTexture);
    m_textureHelper->deleteTexture(&m_selectionResultTexture);

    delete m_shader;
    delete m_depthShader;
    delete m_backgroundShader;
    delete m_selectionShader;
    delete m_surfaceShader;
    delete m_surfaceGridShader;
    delete m_labelShader;

    delete m_backgroundObj;
    delete m_surfaceObj;
    delete m_gridLineObj;
    delete m_labelObj;

    if (m_selectionPointer)
        delete m_selectionPointer;

    for (int i = 0; i < m_dataArray.size(); i++)
        delete m_dataArray.at(i);
    m_dataArray.clear();

}

void Surface3DRenderer::initializeOpenGL()
{
    Abstract3DRenderer::initializeOpenGL();

    // Initialize shaders
    handleShadowQualityChange();

    initSurfaceShaders();

    initLabelShaders(QStringLiteral(":/shaders/vertexLabel"),
                     QStringLiteral(":/shaders/fragmentLabel"));

#if !defined(QT_OPENGL_ES_2)
    // Init depth shader (for shadows). Init in any case, easier to handle shadow activation if done via api.
    initDepthShader();
#endif

    // Init selection shader
    initSelectionShaders();

    // Load grid line mesh
    loadGridLineMesh();

    // Load label mesh
    loadLabelMesh();

    // Set view port
    glViewport(m_mainViewPort.x(), m_mainViewPort.y(),
               m_mainViewPort.width(), m_mainViewPort.height());

    // Resize in case we've missed resize events
    // Resize calls initSelectionBuffer and initDepthBuffer, so they don't need to be called here
    handleResize();

    // Load background mesh (we need to be initialized first)
    loadBackgroundMesh();
}

void Surface3DRenderer::updateDataModel(QSurfaceDataProxy *dataProxy)
{
    for (int i = 0; i < m_dataArray.size(); i++)
        delete m_dataArray.at(i);
    m_dataArray.clear();

    calculateSceneScalingFactors();

    const QSurfaceDataArray *array = dataProxy->array();

    // Need minimum of 2x2 array to draw a surface
    if (array->size() >= 2 && array->at(0)->size() >= 2) {
        QRect sampleSpace = calculateSampleRect(dataProxy);

        bool dimensionChanged = false;
        if (m_sampleSpace != sampleSpace) {
            dimensionChanged = true;
            m_sampleSpace = sampleSpace;
        }

        // TODO: Handle partial surface grids on the graph edges
        if (sampleSpace.width() >= 2 && sampleSpace.height() >= 2) {
            m_dataArray.reserve(sampleSpace.height());
            for (int i = 0; i < sampleSpace.height(); i++) {
                QSurfaceDataRow *newRow = new QSurfaceDataRow();
                newRow->resize(sampleSpace.width());
                for (int j = 0; j < sampleSpace.width(); j++)
                    (*newRow)[j] = array->at(i + sampleSpace.y())->at(j + sampleSpace.x());
                m_dataArray << newRow;
            }

            if (m_dataArray.size() > 0) {
                if (!m_surfaceObj)
                    loadSurfaceObj();

                // Note: Data setup can change samplespace (as min width/height is 1)
                if (m_cachedSmoothSurface)
                    m_surfaceObj->setUpSmoothData(m_dataArray, m_sampleSpace, m_heightNormalizer, dimensionChanged);
                else
                    m_surfaceObj->setUpData(m_dataArray, m_sampleSpace, m_heightNormalizer, dimensionChanged);

                if (dimensionChanged)
                    updateSelectionTexture();
            }
        }
    }

    Abstract3DRenderer::updateDataModel(dataProxy);
}

QRect Surface3DRenderer::calculateSampleRect(QSurfaceDataProxy *dataProxy)
{
    QRect sampleSpace;

    m_minVisibleColumnValue = dataProxy->minValueColumns();
    m_maxVisibleColumnValue = dataProxy->maxValueColumns();
    m_minVisibleRowValue = dataProxy->minValueRows();
    m_maxVisibleRowValue = dataProxy->maxValueRows();
    m_dataStepX = (m_maxVisibleColumnValue - m_minVisibleColumnValue) / (dataProxy->columnCount() - 1);
    m_dataStepZ = (m_maxVisibleRowValue - m_minVisibleRowValue) / (dataProxy->rowCount() - 1);

    if (m_minVisibleColumnValue >  m_axisCacheX.max() || m_maxVisibleColumnValue <  m_axisCacheX.min()
            || m_minVisibleRowValue >  m_axisCacheZ.max() || m_maxVisibleRowValue <  m_axisCacheZ.min()) {
        sampleSpace.setWidth(-1); // to indicate nothing needs to be shown
    }

    int index = 0;
    while (m_minVisibleColumnValue < m_axisCacheX.min()) {
        m_minVisibleColumnValue += m_dataStepX;
        index++;
    }
    sampleSpace.setLeft(index);

    index = dataProxy->columnCount() - 1;
    while (m_maxVisibleColumnValue > m_axisCacheX.max()) {
        m_maxVisibleColumnValue -= m_dataStepX;
        index--;
    }
    sampleSpace.setRight(index);

    index = 0;
    while (m_minVisibleRowValue < m_axisCacheZ.min()) {
        m_minVisibleRowValue += m_dataStepZ;
        index++;
    }
    sampleSpace.setTop(index);

    index = dataProxy->rowCount() - 1;
    while (m_maxVisibleRowValue > m_axisCacheZ.max()) {
        m_maxVisibleRowValue -= m_dataStepZ;
        index--;
    }
    sampleSpace.setBottom(index);

    m_surfaceScaleX = m_scaleX * (m_maxVisibleColumnValue - m_minVisibleColumnValue) / m_areaSize.width();
    m_surfaceScaleZ = m_scaleZ * (m_maxVisibleRowValue - m_minVisibleRowValue) / m_areaSize.height();
    GLfloat axis2XCenterX = (m_axisCacheX.min() + m_axisCacheX.max());
    GLfloat axis2XCenterZ = (m_axisCacheZ.min() + m_axisCacheZ.max());
    GLfloat data2XCenterX = (m_minVisibleColumnValue + m_maxVisibleColumnValue);
    GLfloat data2XCenterZ = (m_minVisibleRowValue + m_maxVisibleRowValue);
    m_surfaceOffsetX = m_scaleX * (data2XCenterX - axis2XCenterX) / m_areaSize.width();
    m_surfaceOffsetZ = -m_scaleZ * (data2XCenterZ - axis2XCenterZ) / m_areaSize.height() + zComp;

    return sampleSpace;
}

void Surface3DRenderer::updateScene(Q3DScene *scene)
{
    // TODO: Move these to more suitable place e.g. controller should be controlling the viewports.
    scene->setPrimarySubViewport(m_mainViewPort);
    scene->setUnderSideCameraEnabled(false);

    // Set initial camera position
    // X must be 0 for rotation to work - we can use "setCameraRotation" for setting it later
    if (m_hasHeightAdjustmentChanged) {
        scene->activeCamera()->setBaseOrientation(QVector3D(0.0f, 0.0f, cameraDistance + zComp),
                                                  QVector3D(0.0f, 0.0f, zComp),
                                                  QVector3D(0.0f, 1.0f, 0.0f));
        // For now this is used just to make things once. Proper use will come
        m_hasHeightAdjustmentChanged = false;
    }

    scene->activeCamera()->d_ptr->updateViewMatrix(m_autoScaleAdjustment);
    scene->setLightPositionRelativeToCamera(defaultLightPos);

    if (m_selectionPointer)
        m_selectionPointer->updateScene(scene);

    Abstract3DRenderer::updateScene(scene);
}

void Surface3DRenderer::render(GLuint defaultFboHandle)
{
    m_cachedScene->setUnderSideCameraEnabled(false);
    // Handle GL state setup for FBO buffers and clearing of the render surface
    Abstract3DRenderer::render(defaultFboHandle);

    // Draw the surface scene
    drawScene(defaultFboHandle);

    // If selection pointer is active, pass the render request for it also
    if (m_selectionPointer && m_selectionActive)
        m_selectionPointer->render(defaultFboHandle);
}

void Surface3DRenderer::drawScene(GLuint defaultFboHandle)
{
    GLfloat backgroundRotation = 0;

    // Specify viewport
    glViewport(m_mainViewPort.x(), m_mainViewPort.y(),
               m_mainViewPort.width(), m_mainViewPort.height());

    // Set up projection matrix
    QMatrix4x4 projectionMatrix;
    projectionMatrix.perspective(45.0f, (GLfloat)m_mainViewPort.width()
                                 / (GLfloat)m_mainViewPort.height(), 0.1f, 100.0f);

    // Calculate view matrix
    QMatrix4x4 viewMatrix = m_cachedScene->activeCamera()->viewMatrix();

    QMatrix4x4 projectionViewMatrix = projectionMatrix * viewMatrix;

    // Calculate flipping indicators
    if (viewMatrix.row(0).x() > 0)
        m_zFlipped = false;
    else
        m_zFlipped = true;
    if (viewMatrix.row(0).z() <= 0)
        m_xFlipped = false;
    else
        m_xFlipped = true;

    // calculate background rotation based on view matrix rotation
    if (viewMatrix.row(0).x() > 0 && viewMatrix.row(0).z() <= 0)
        backgroundRotation = 270.0f;
    else if (viewMatrix.row(0).x() > 0 && viewMatrix.row(0).z() > 0)
        backgroundRotation = 180.0f;
    else if (viewMatrix.row(0).x() <= 0 && viewMatrix.row(0).z() > 0)
        backgroundRotation = 90.0f;
    else if (viewMatrix.row(0).x() <= 0 && viewMatrix.row(0).z() <= 0)
        backgroundRotation = 0.0f;

    QVector3D lightPos = m_cachedScene->activeLight()->position();

    QMatrix4x4 depthViewMatrix;
    QMatrix4x4 depthProjectionMatrix;
    QMatrix4x4 depthProjectionViewMatrix;

    GLfloat adjustedLightStrength = m_cachedTheme.m_lightStrength / 10.0f;

    //
    //  Do the surface drawing
    //

    QVector3D surfaceScaler(m_surfaceScaleX, 1.0f, m_surfaceScaleZ);
    QVector3D surfaceOffset(m_surfaceOffsetX, 0.0f, m_surfaceOffsetZ);

    // Draw depth buffer
#if !defined(QT_OPENGL_ES_2)
    if (m_cachedShadowQuality > QDataVis::ShadowNone && m_surfaceObj) {
        // Render scene into a depth texture for using with shadow mapping
        // Enable drawing to depth framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, m_depthFrameBuffer);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Bind depth shader
        m_depthShader->bind();

        // Set viewport for depth map rendering. Must match texture size. Larger values give smoother shadows.
        glViewport(m_mainViewPort.x(), m_mainViewPort.y(),
                   m_mainViewPort.width() * m_shadowQualityMultiplier,
                   m_mainViewPort.height() * m_shadowQualityMultiplier);

        // Get the depth view matrix
        // It may be possible to hack lightPos here if we want to make some tweaks to shadow
        QVector3D depthLightPos = m_cachedScene->activeCamera()->calculatePositionRelativeToCamera(
                    QVector3D(0.0f, 0.0f, zComp), 0.0f, 1.5f / m_autoScaleAdjustment);
        depthViewMatrix.lookAt(depthLightPos, QVector3D(0.0f, 0.0f, zComp),
                               QVector3D(0.0f, 1.0f, 0.0f));

        // TODO: Why does depthViewMatrix.column(3).y() goes to zero when we're directly above?
        // That causes the scene to be not drawn from above -> must be fixed
        // qDebug() << lightPos << depthViewMatrix << depthViewMatrix.column(3);
        // Set the depth projection matrix
#ifndef USE_WIDER_SHADOWS
        // Use this for perspective shadows
        depthProjectionMatrix.perspective(10.0f, (GLfloat)m_mainViewPort.width()
                                          / (GLfloat)m_mainViewPort.height(), 3.0f, 100.0f);
#else
        // Use these for orthographic shadows
        depthProjectionMatrix.ortho(-2.0f * 2.0f, 2.0f * 2.0f,
                                    -2.0f, 2.0f,
                                    0.0f, 100.0f);
#endif
        depthProjectionViewMatrix = depthProjectionMatrix * depthViewMatrix;

        glCullFace(GL_FRONT);

        QMatrix4x4 modelMatrix;
        QMatrix4x4 MVPMatrix;

        modelMatrix.translate(surfaceOffset);
        modelMatrix.scale(surfaceScaler);

        MVPMatrix = depthProjectionViewMatrix * modelMatrix;

        m_depthShader->setUniformValue(m_depthShader->MVP(), MVPMatrix);

        // 1st attribute buffer : vertices
        glEnableVertexAttribArray(m_depthShader->posAtt());
        glBindBuffer(GL_ARRAY_BUFFER, m_surfaceObj->vertexBuf());
        glVertexAttribPointer(m_depthShader->posAtt(), 3, GL_FLOAT, GL_FALSE, 0,
                              (void *)0);

        // Index buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_surfaceObj->elementBuf());

        // Draw the triangles
        glDrawElements(GL_TRIANGLES, m_surfaceObj->indexCount(), GL_UNSIGNED_SHORT,
                       (void *)0);

        // Free buffers
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glDisableVertexAttribArray(m_depthShader->posAtt());

        // Disable drawing to depth framebuffer (= enable drawing to screen)
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFboHandle);

        // Release depth shader
        m_depthShader->release();

        // Revert to original viewport
        glViewport(m_mainViewPort.x(), m_mainViewPort.y(),
                   m_mainViewPort.width(), m_mainViewPort.height());

        // Reset culling to normal
        glCullFace(GL_BACK);

#if 0 // Use this if you want to see what is being drawn to the framebuffer
        // You'll also have to comment out GL_COMPARE_R_TO_TEXTURE -line in texturehelper (if using it)
        {
            m_labelShader->bind();
            glEnable(GL_TEXTURE_2D);
            QMatrix4x4 modelMatrix;
            QMatrix4x4 viewmatrix;
            viewmatrix.lookAt(QVector3D(0.0f, 0.0f, 2.5f + zComp),
                              QVector3D(0.0f, 0.0f, zComp),
                              QVector3D(0.0f, 1.0f, 0.0f));
            modelMatrix.translate(0.0, 0.0, zComp);
            QMatrix4x4 MVPMatrix = projectionMatrix * viewmatrix * modelMatrix;
            m_labelShader->setUniformValue(m_labelShader->MVP(), MVPMatrix);
            m_drawer->drawObject(m_labelShader, m_labelObj, m_depthTexture);
            glDisable(GL_TEXTURE_2D);
            m_labelShader->release();
        }
#endif
    }
#endif

    // Enable texturing
    glEnable(GL_TEXTURE_2D);

    // Draw selection buffer
    if (m_querySelection && m_surfaceObj && m_sampleSpace.width() >= 2 && m_sampleSpace.height() >= 2) {
        m_selectionShader->bind();
        glBindFramebuffer(GL_FRAMEBUFFER, m_selectionFrameBuffer);
        glEnable(GL_DEPTH_TEST); // Needed, otherwise the depth render buffer is not used
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Needed for clearing the frame buffer
        glDisable(GL_DITHER); // disable dithering, it may affect colors if enabled

        glDisable(GL_CULL_FACE);

        QMatrix4x4 modelMatrix;
        QMatrix4x4 MVPMatrix;

        modelMatrix.translate(surfaceOffset);
        modelMatrix.scale(surfaceScaler);

        MVPMatrix = projectionViewMatrix * modelMatrix;

        m_selectionShader->setUniformValue(m_selectionShader->MVP(), MVPMatrix);

        m_drawer->drawObject(m_selectionShader, m_surfaceObj, m_selectionTexture);

        glEnable(GL_DITHER);

        m_querySelection = false;

        QPoint point = m_controller->inputPosition();
        GLubyte pixel[4] = {0};
        glReadPixels(point.x(), m_cachedBoundingRect.height() - point.y(), 1, 1,
                     GL_RGBA, GL_UNSIGNED_BYTE, (void *)pixel);

        glBindFramebuffer(GL_FRAMEBUFFER, defaultFboHandle);

        // Release selection shader
        m_selectionShader->release();

        // Put the RGBA value back to uint
        uint id = pixel[0] + pixel[1] * 256 + pixel[2] * 65536 + pixel[3] * 16777216;
        if (id) {
            surfacePointSelected(id);
        } else {
            //surfacePointCleared();
            m_selectionActive = false;
        }
        glEnable(GL_CULL_FACE);
    }

    if (m_surfaceObj && m_sampleSpace.width() >= 2 && m_sampleSpace.height() >= 2) {
        m_surfaceShader->bind();
        // m_selectionShader->bind(); // IFDEF print selection

        // For surface we can see climpses from underneath
        glDisable(GL_CULL_FACE);

        QMatrix4x4 modelMatrix;
        QMatrix4x4 MVPMatrix;
        QMatrix4x4 depthMVPMatrix;
        QMatrix4x4 itModelMatrix;

        modelMatrix.translate(surfaceOffset);
        modelMatrix.scale(surfaceScaler);
        itModelMatrix.scale(surfaceScaler);

#ifdef SHOW_DEPTH_TEXTURE_SCENE
        MVPMatrix = depthProjectionViewMatrix * modelMatrix;
#else
        MVPMatrix = projectionViewMatrix * modelMatrix;
#endif
        depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;

        // Set shader bindings
        m_surfaceShader->setUniformValue(m_surfaceShader->lightP(), lightPos);
        m_surfaceShader->setUniformValue(m_surfaceShader->view(), viewMatrix);
        m_surfaceShader->setUniformValue(m_surfaceShader->model(), modelMatrix);
        m_surfaceShader->setUniformValue(m_surfaceShader->nModel(),
                                         itModelMatrix.inverted().transposed());
        m_surfaceShader->setUniformValue(m_surfaceShader->MVP(), MVPMatrix);
        m_surfaceShader->setUniformValue(m_surfaceShader->ambientS(),
                                         m_cachedTheme.m_ambientStrength);

#if !defined(QT_OPENGL_ES_2)
        if (m_cachedShadowQuality > QDataVis::ShadowNone) {
            // Set shadow shader bindings
            m_surfaceShader->setUniformValue(m_surfaceShader->shadowQ(), m_shadowQualityToShader);
            m_surfaceShader->setUniformValue(m_surfaceShader->depth(), depthMVPMatrix);
            m_surfaceShader->setUniformValue(m_surfaceShader->lightS(), adjustedLightStrength);

            // Draw the object
            m_drawer->drawObject(m_surfaceShader, m_surfaceObj, m_gradientTexture, m_depthTexture);
        } else
#endif
        {
            // Set shadowless shader bindings
            m_surfaceShader->setUniformValue(m_surfaceShader->lightS(),
                                             m_cachedTheme.m_lightStrength);

            // Draw the object
            m_drawer->drawObject(m_surfaceShader, m_surfaceObj, m_gradientTexture);
        }

        m_surfaceShader->release();

        // Draw surface grid
        if (m_cachedSurfaceGridOn) {
            m_surfaceGridShader->bind();

            m_surfaceGridShader->setUniformValue(m_surfaceGridShader->color(),
                                                 Utils::vectorFromColor(m_cachedTheme.m_gridLine));
            // Draw the grid twice, with slight offset on Y axis to each direction
            for (int i = 0; i < 2; i++) {
                MVPMatrix.translate(0.0f, surfaceGridYOffset[i], 0.0f);

                m_surfaceGridShader->setUniformValue(m_surfaceGridShader->MVP(), MVPMatrix);
                m_drawer->drawSurfaceGrid(m_surfaceGridShader, m_surfaceObj);
            }
            m_surfaceGridShader->release();
        }
        glEnable(GL_CULL_FACE);
    }

    // Bind background shader
    m_backgroundShader->bind();
    glCullFace(GL_BACK);

    // Draw background
    if (m_cachedIsBackgroundEnabled && m_backgroundObj) {
        QMatrix4x4 modelMatrix;
        QMatrix4x4 MVPMatrix;
        QMatrix4x4 depthMVPMatrix;
        QMatrix4x4 itModelMatrix;

        modelMatrix.translate(0.0f, 0.0f, zComp);
        QVector3D bgScale(m_scaleXWithBackground, backgroundMargin, m_scaleZWithBackground);
        modelMatrix.scale(bgScale);
        itModelMatrix.scale(bgScale);

        // If we're viewing from below, background object must be flipped
        if (m_yFlipped) {
            modelMatrix.rotate(180.0f, 1.0, 0.0, 0.0);
            modelMatrix.rotate(270.0f - backgroundRotation, 0.0f, 1.0f, 0.0f);
        } else {
            modelMatrix.rotate(backgroundRotation, 0.0f, 1.0f, 0.0f);
        }

#ifdef SHOW_DEPTH_TEXTURE_SCENE
        MVPMatrix = depthProjectionViewMatrix * modelMatrix;
#else
        MVPMatrix = projectionViewMatrix * modelMatrix;
#endif
        depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;

        QVector3D backgroundColor = Utils::vectorFromColor(m_cachedTheme.m_backgroundColor);

        // Set shader bindings
        m_backgroundShader->setUniformValue(m_backgroundShader->lightP(), lightPos);
        m_backgroundShader->setUniformValue(m_backgroundShader->view(), viewMatrix);
        m_backgroundShader->setUniformValue(m_backgroundShader->model(), modelMatrix);
        m_backgroundShader->setUniformValue(m_backgroundShader->nModel(),
                                            itModelMatrix.inverted().transposed());
        m_backgroundShader->setUniformValue(m_backgroundShader->MVP(), MVPMatrix);
        m_backgroundShader->setUniformValue(m_backgroundShader->color(), backgroundColor);
        m_backgroundShader->setUniformValue(m_backgroundShader->ambientS(),
                                            m_cachedTheme.m_ambientStrength * 2.0f);

#if !defined(QT_OPENGL_ES_2)
        if (m_cachedShadowQuality > QDataVis::ShadowNone) {
            // Set shadow shader bindings
            m_backgroundShader->setUniformValue(m_backgroundShader->shadowQ(),
                                                m_shadowQualityToShader);
            m_backgroundShader->setUniformValue(m_backgroundShader->depth(), depthMVPMatrix);
            m_backgroundShader->setUniformValue(m_backgroundShader->lightS(),
                                                adjustedLightStrength);

            // Draw the object
            m_drawer->drawObject(m_backgroundShader, m_backgroundObj, 0, m_depthTexture);
        } else
#endif
        {
            // Set shadowless shader bindings
            m_backgroundShader->setUniformValue(m_backgroundShader->lightS(),
                                                m_cachedTheme.m_lightStrength);

            // Draw the object
            m_drawer->drawObject(m_backgroundShader, m_backgroundObj);
        }
    }

    // Release background shader
    m_backgroundShader->release();

    // Draw grid lines
    QVector3D gridLineScaleX(m_scaleXWithBackground, gridLineWidth, gridLineWidth);
    QVector3D gridLineScaleZ(gridLineWidth, gridLineWidth, m_scaleZWithBackground);
    QVector3D gridLineScaleY(gridLineWidth, backgroundMargin, gridLineWidth);

    if (m_cachedIsGridEnabled && m_heightNormalizer) {
        ShaderHelper *lineShader = m_backgroundShader;
        // Bind line shader
        lineShader->bind();

        // Set unchanging shader bindings
        QVector3D lineColor = Utils::vectorFromColor(m_cachedTheme.m_gridLine);
        lineShader->setUniformValue(lineShader->lightP(), lightPos);
        lineShader->setUniformValue(lineShader->view(), viewMatrix);
        lineShader->setUniformValue(lineShader->color(), lineColor);
        lineShader->setUniformValue(lineShader->ambientS(), m_cachedTheme.m_ambientStrength);

        // Rows (= Z)
        if (m_axisCacheZ.segmentCount() > 0) {
            // Floor lines
            GLfloat lineStep = 2.0f * aspectRatio * m_axisCacheZ.subSegmentStep() / m_scaleFactor;
            GLfloat linePos = m_scaleZ + zComp; // Start line
            int lastSegment = m_axisCacheZ.subSegmentCount() * m_axisCacheZ.segmentCount();

            for (int segment = 0; segment <= lastSegment; segment++) {
                QMatrix4x4 modelMatrix;
                QMatrix4x4 MVPMatrix;
                QMatrix4x4 depthMVPMatrix;
                QMatrix4x4 itModelMatrix;

                if (m_yFlipped)
                    modelMatrix.translate(0.0f, backgroundMargin, linePos);
                else
                    modelMatrix.translate(0.0f, -backgroundMargin, linePos);

                modelMatrix.scale(gridLineScaleX);
                itModelMatrix.scale(gridLineScaleX);

                // If we're viewing from below, grid line object must be flipped
                if (m_yFlipped)
                    modelMatrix.rotate(180.0f, 1.0, 0.0, 0.0);

                MVPMatrix = projectionViewMatrix * modelMatrix;
                depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;

                // Set the rest of the shader bindings
                lineShader->setUniformValue(lineShader->model(), modelMatrix);
                lineShader->setUniformValue(lineShader->nModel(),
                                            itModelMatrix.inverted().transposed());
                lineShader->setUniformValue(lineShader->MVP(), MVPMatrix);

#if !defined(QT_OPENGL_ES_2)
                if (m_cachedShadowQuality > QDataVis::ShadowNone) {
                    // Set shadow shader bindings
                    lineShader->setUniformValue(lineShader->shadowQ(), m_shadowQualityToShader);
                    lineShader->setUniformValue(lineShader->depth(), depthMVPMatrix);
                    lineShader->setUniformValue(lineShader->lightS(), adjustedLightStrength);

                    // Draw the object
                    m_drawer->drawObject(lineShader, m_gridLineObj, 0, m_depthTexture);
                } else
#endif
                {
                    // Set shadowless shader bindings
                    lineShader->setUniformValue(lineShader->lightS(),
                                                m_cachedTheme.m_lightStrength);

                    // Draw the object
                    m_drawer->drawObject(lineShader, m_gridLineObj);
                }
                linePos -= lineStep;
            }

            // Side wall lines
            GLfloat lineXTrans = m_scaleXWithBackground;
            linePos = m_scaleZ + zComp; // Start line

            if (!m_xFlipped)
                lineXTrans = -lineXTrans;

            for (int segment = 0; segment <= lastSegment; segment++) {
                QMatrix4x4 modelMatrix;
                QMatrix4x4 MVPMatrix;
                QMatrix4x4 depthMVPMatrix;
                QMatrix4x4 itModelMatrix;

                modelMatrix.translate(lineXTrans, 0.0f, linePos);
                modelMatrix.scale(gridLineScaleY);
                itModelMatrix.scale(gridLineScaleY);

                MVPMatrix = projectionViewMatrix * modelMatrix;
                depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;

                // Set the rest of the shader bindings
                lineShader->setUniformValue(lineShader->model(), modelMatrix);
                lineShader->setUniformValue(lineShader->nModel(),
                                            itModelMatrix.inverted().transposed());
                lineShader->setUniformValue(lineShader->MVP(), MVPMatrix);

#if !defined(QT_OPENGL_ES_2)
                if (m_cachedShadowQuality > QDataVis::ShadowNone) {
                    // Set shadow shader bindings
                    lineShader->setUniformValue(lineShader->shadowQ(), m_shadowQualityToShader);
                    lineShader->setUniformValue(lineShader->depth(), depthMVPMatrix);
                    lineShader->setUniformValue(lineShader->lightS(),
                                                adjustedLightStrength);

                    // Draw the object
                    m_drawer->drawObject(lineShader, m_gridLineObj, 0, m_depthTexture);
                } else
#endif
                {
                    // Set shadowless shader bindings
                    lineShader->setUniformValue(lineShader->lightS(),
                                                m_cachedTheme.m_lightStrength);

                    // Draw the object
                    m_drawer->drawObject(lineShader, m_gridLineObj);
                }
                linePos -= lineStep;
            }
        }

        // Columns (= X)
        if (m_axisCacheX.segmentCount() > 0) {
            // Floor lines
            GLfloat lineStep = -2.0f * aspectRatio * m_axisCacheX.subSegmentStep() / m_scaleFactor;
            GLfloat linePos = m_scaleX;
            int lastSegment = m_axisCacheX.subSegmentCount() * m_axisCacheX.segmentCount();

            for (int segment = 0; segment <= lastSegment; segment++) {
                QMatrix4x4 modelMatrix;
                QMatrix4x4 MVPMatrix;
                QMatrix4x4 depthMVPMatrix;
                QMatrix4x4 itModelMatrix;

                if (m_yFlipped)
                    modelMatrix.translate(linePos, backgroundMargin, zComp);
                else
                    modelMatrix.translate(linePos, -backgroundMargin, zComp);

                modelMatrix.scale(gridLineScaleZ);
                itModelMatrix.scale(gridLineScaleZ);

                // If we're viewing from below, grid line object must be flipped
                if (m_yFlipped)
                    modelMatrix.rotate(180.0f, 1.0, 0.0, 0.0);

                MVPMatrix = projectionViewMatrix * modelMatrix;
                depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;

                // Set the rest of the shader bindings
                lineShader->setUniformValue(lineShader->model(), modelMatrix);
                lineShader->setUniformValue(lineShader->nModel(),
                                            itModelMatrix.inverted().transposed());
                lineShader->setUniformValue(lineShader->MVP(), MVPMatrix);

#if !defined(QT_OPENGL_ES_2)
                if (m_cachedShadowQuality > QDataVis::ShadowNone) {
                    // Set shadow shader bindings
                    lineShader->setUniformValue(lineShader->shadowQ(), m_shadowQualityToShader);
                    lineShader->setUniformValue(lineShader->depth(), depthMVPMatrix);
                    lineShader->setUniformValue(lineShader->lightS(), adjustedLightStrength);

                    // Draw the object
                    m_drawer->drawObject(lineShader, m_gridLineObj, 0, m_depthTexture);
                } else
#endif
                {
                    // Set shadowless shader bindings
                    lineShader->setUniformValue(lineShader->lightS(),
                                                m_cachedTheme.m_lightStrength);

                    // Draw the object
                    m_drawer->drawObject(lineShader, m_gridLineObj);
                }
                linePos += lineStep;
            }

            // Back wall lines
            GLfloat lineZTrans = m_scaleZWithBackground + zComp;
            linePos = m_scaleX;

            if (!m_zFlipped)
                lineZTrans = -lineZTrans + zComp + zComp;

            for (int segment = 0; segment <= lastSegment; segment++) {
                QMatrix4x4 modelMatrix;
                QMatrix4x4 MVPMatrix;
                QMatrix4x4 depthMVPMatrix;
                QMatrix4x4 itModelMatrix;

                modelMatrix.translate(linePos, 0.0f, lineZTrans);
                modelMatrix.scale(gridLineScaleY);
                itModelMatrix.scale(gridLineScaleY);

                MVPMatrix = projectionViewMatrix * modelMatrix;
                depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;

                // Set the rest of the shader bindings
                lineShader->setUniformValue(lineShader->model(), modelMatrix);
                lineShader->setUniformValue(lineShader->nModel(),
                                            itModelMatrix.inverted().transposed());
                lineShader->setUniformValue(lineShader->MVP(), MVPMatrix);

#if !defined(QT_OPENGL_ES_2)
                if (m_cachedShadowQuality > QDataVis::ShadowNone) {
                    // Set shadow shader bindings
                    lineShader->setUniformValue(lineShader->shadowQ(), m_shadowQualityToShader);
                    lineShader->setUniformValue(lineShader->depth(), depthMVPMatrix);
                    lineShader->setUniformValue(lineShader->lightS(), adjustedLightStrength);

                    // Draw the object
                    m_drawer->drawObject(lineShader, m_gridLineObj, 0, m_depthTexture);
                } else
#endif
                {
                    // Set shadowless shader bindings
                    lineShader->setUniformValue(lineShader->lightS(),
                                                m_cachedTheme.m_lightStrength);

                    // Draw the object
                    m_drawer->drawObject(lineShader, m_gridLineObj);
                }
                linePos += lineStep;
            }
        }

        // Horizontal wall lines
        if (m_axisCacheY.segmentCount() > 0) {
            // Back wall
            GLfloat lineStep = 2.0f * m_axisCacheY.subSegmentStep() / m_heightNormalizer;
            GLfloat linePos = -1.0f;
            int lastSegment = m_axisCacheY.subSegmentCount() * m_axisCacheY.segmentCount();

            GLfloat lineZTrans = m_scaleZWithBackground + zComp;

            if (!m_zFlipped)
                lineZTrans = -lineZTrans + zComp + zComp;

            for (int segment = 0; segment <= lastSegment; segment++) {
                QMatrix4x4 modelMatrix;
                QMatrix4x4 MVPMatrix;
                QMatrix4x4 depthMVPMatrix;
                QMatrix4x4 itModelMatrix;

                modelMatrix.translate(0.0f, linePos, lineZTrans);

                modelMatrix.scale(gridLineScaleX);
                itModelMatrix.scale(gridLineScaleX);

                MVPMatrix = projectionViewMatrix * modelMatrix;
                depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;

                // Set the rest of the shader bindings
                lineShader->setUniformValue(lineShader->model(), modelMatrix);
                lineShader->setUniformValue(lineShader->nModel(),
                                            itModelMatrix.inverted().transposed());
                lineShader->setUniformValue(lineShader->MVP(), MVPMatrix);

#if !defined(QT_OPENGL_ES_2)
                if (m_cachedShadowQuality > QDataVis::ShadowNone) {
                    // Set shadow shader bindings
                    lineShader->setUniformValue(lineShader->shadowQ(), m_shadowQualityToShader);
                    lineShader->setUniformValue(lineShader->depth(), depthMVPMatrix);
                    lineShader->setUniformValue(lineShader->lightS(), adjustedLightStrength);

                    // Draw the object
                    m_drawer->drawObject(lineShader, m_gridLineObj, 0, m_depthTexture);
                } else
#endif
                {
                    // Set shadowless shader bindings
                    lineShader->setUniformValue(lineShader->lightS(),
                                                m_cachedTheme.m_lightStrength);

                    // Draw the object
                    m_drawer->drawObject(lineShader, m_gridLineObj);
                }
                linePos += lineStep;
            }

            // Side wall
            linePos = -1.0f;
            lastSegment = m_axisCacheY.subSegmentCount() * m_axisCacheY.segmentCount();
            GLfloat lineXTrans = m_scaleXWithBackground;

            if (!m_xFlipped)
                lineXTrans = -lineXTrans;

            for (int segment = 0; segment <= lastSegment; segment++) {
                QMatrix4x4 modelMatrix;
                QMatrix4x4 MVPMatrix;
                QMatrix4x4 depthMVPMatrix;
                QMatrix4x4 itModelMatrix;

                modelMatrix.translate(lineXTrans, linePos, zComp);

                modelMatrix.scale(gridLineScaleZ);
                itModelMatrix.scale(gridLineScaleZ);

                MVPMatrix = projectionViewMatrix * modelMatrix;
                depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;

                // Set the rest of the shader bindings
                lineShader->setUniformValue(lineShader->model(), modelMatrix);
                lineShader->setUniformValue(lineShader->nModel(),
                                            itModelMatrix.inverted().transposed());
                lineShader->setUniformValue(lineShader->MVP(), MVPMatrix);

#if !defined(QT_OPENGL_ES_2)
                if (m_cachedShadowQuality > QDataVis::ShadowNone) {
                    // Set shadow shader bindings
                    lineShader->setUniformValue(lineShader->shadowQ(), m_shadowQualityToShader);
                    lineShader->setUniformValue(lineShader->depth(), depthMVPMatrix);
                    lineShader->setUniformValue(lineShader->lightS(), adjustedLightStrength);

                    // Draw the object
                    m_drawer->drawObject(lineShader, m_gridLineObj, 0, m_depthTexture);
                } else
#endif
                {
                    // Set shadowless shader bindings
                    lineShader->setUniformValue(lineShader->lightS(),
                                                m_cachedTheme.m_lightStrength);

                    // Draw the object
                    m_drawer->drawObject(lineShader, m_gridLineObj);
                }
                linePos += lineStep;
            }
        }

        // Release line shader
        lineShader->release();
    }

    // Draw axis labels
    m_labelShader->bind();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Z Labels
    QVector3D positionZComp(0.0f, 0.0f, zComp);
    if (m_axisCacheZ.segmentCount() > 0) {
        GLfloat posStep = 2.0f * aspectRatio * m_axisCacheZ.segmentStep() / m_scaleFactor;
        GLfloat labelPos = m_scaleZ + zComp;
        int lastSegment = m_axisCacheZ.segmentCount();
        int labelNbr = 0;
        GLfloat labelXTrans = m_scaleXWithBackground + labelMargin;
        GLfloat labelYTrans = -backgroundMargin;
        GLfloat rotLabelX = -90.0f;
        GLfloat rotLabelY = 0.0f;
        GLfloat rotLabelZ = 0.0f;
        Qt::AlignmentFlag alignment = Qt::AlignRight;
        if (m_zFlipped)
            rotLabelY = 180.0f;
        if (m_xFlipped) {
            labelXTrans = -labelXTrans;
            alignment = Qt::AlignLeft;
        }
        if (m_yFlipped) {
            rotLabelZ += 180.0f;
            rotLabelY += 180.0f;
            labelYTrans = -labelYTrans;
        }
        QVector3D labelTrans = QVector3D(labelXTrans,
                                         labelYTrans,
                                         labelPos);
        QVector3D rotation(rotLabelX, rotLabelY, rotLabelZ);

        for (int segment = 0; segment <= lastSegment; segment++) {
            if (m_axisCacheZ.labelItems().size() > labelNbr) {
                labelTrans.setZ(labelPos);

                // Draw the label here
                m_dummyRenderItem.setTranslation(labelTrans);
                const LabelItem &axisLabelItem = *m_axisCacheZ.labelItems().at(labelNbr);

                m_drawer->drawLabel(m_dummyRenderItem, axisLabelItem, viewMatrix, projectionMatrix,
                                    positionZComp, rotation, 0, m_cachedSelectionMode,
                                    m_labelShader, m_labelObj, m_cachedScene->activeCamera(),
                                    true, true, Drawer::LabelMid, alignment);
            }
            labelNbr++;
            labelPos -= posStep;
        }
    }
    // X Labels
    if (m_axisCacheX.segmentCount() > 0) {
        GLfloat posStep = 2.0f * aspectRatio * m_axisCacheX.segmentStep() / m_scaleFactor;
        GLfloat labelPos = -m_scaleX;
        int lastSegment = m_axisCacheX.segmentCount();

        int labelNbr = 0;
        GLfloat labelZTrans = m_scaleZWithBackground + labelMargin;
        GLfloat labelYTrans = -backgroundMargin;
        GLfloat rotLabelX = -90.0f;
        GLfloat rotLabelY = 90.0f;
        GLfloat rotLabelZ = 0.0f;
        Qt::AlignmentFlag alignment = Qt::AlignLeft;
        if (m_xFlipped)
            rotLabelY = -90.0f;
        if (m_zFlipped) {
            labelZTrans = -labelZTrans;
            alignment = Qt::AlignRight;
        }
        if (m_yFlipped) {
            rotLabelZ += 180.0f;
            rotLabelY += 180.0f;
            labelYTrans = -labelYTrans;
        }
        QVector3D labelTrans = QVector3D(labelPos,
                                         labelYTrans,
                                         labelZTrans + zComp);
        QVector3D rotation(rotLabelX, rotLabelY, rotLabelZ);

        for (int segment = 0; segment <= lastSegment; segment++) {
            if (m_axisCacheX.labelItems().size() > labelNbr) {
                // Draw the label here
                labelTrans.setX(labelPos);
                m_dummyRenderItem.setTranslation(labelTrans);
                const LabelItem &axisLabelItem = *m_axisCacheX.labelItems().at(labelNbr);

                m_drawer->drawLabel(m_dummyRenderItem, axisLabelItem, viewMatrix, projectionMatrix,
                                    positionZComp, rotation, 0, m_cachedSelectionMode,
                                    m_labelShader, m_labelObj, m_cachedScene->activeCamera(),
                                    true, true, Drawer::LabelMid, alignment);
            }
            labelNbr++;
            labelPos += posStep;
        }
    }
    // Y Labels
    if (m_axisCacheY.segmentCount() > 0) {
        GLfloat posStep = 2.0f * m_axisCacheY.segmentStep() / m_heightNormalizer;
        GLfloat labelPos = -1.0f;
        int labelNbr = 0;
        GLfloat labelXTrans = m_scaleXWithBackground;
        GLfloat labelZTrans = m_scaleZWithBackground;

        GLfloat labelMarginXTrans = labelMargin;
        GLfloat labelMarginZTrans = labelMargin;
        GLfloat rotLabelX = 0.0f;
        GLfloat rotLabelY = -90.0f;
        GLfloat rotLabelZ = 0.0f;
        Qt::AlignmentFlag alignment = Qt::AlignLeft;
        if (!m_xFlipped) {
            labelXTrans = -labelXTrans;
            labelMarginXTrans = -labelMargin;
            rotLabelY = 90.0f;
        }
        if (m_zFlipped) {
            labelZTrans = -labelZTrans;
            labelMarginZTrans = -labelMargin;
            alignment = Qt::AlignRight;
        }

        // Back wall
        QVector3D rotation(rotLabelX, rotLabelY, rotLabelZ);

        for (int segment = 0; segment <= m_axisCacheY.segmentCount(); segment++) {
            if (m_axisCacheY.labelItems().size() > labelNbr) {
                const LabelItem &axisLabelItem = *m_axisCacheY.labelItems().at(labelNbr);

                QVector3D labelTrans = QVector3D(labelXTrans, labelPos,
                                                 labelZTrans + labelMarginZTrans + zComp);
                if (m_xFlipped)
                    rotation.setY(-90.0f);
                else
                    rotation.setY(90.0f);
                if (m_zFlipped)
                    alignment = Qt::AlignRight;
                else
                    alignment = Qt::AlignLeft;

                // Draw the label here
                m_dummyRenderItem.setTranslation(labelTrans);
                m_drawer->drawLabel(m_dummyRenderItem, axisLabelItem, viewMatrix, projectionMatrix,
                                    positionZComp, rotation, 0, m_cachedSelectionMode,
                                    m_labelShader, m_labelObj, m_cachedScene->activeCamera(),
                                    true, true, Drawer::LabelMid, alignment);

                // Side wall
                if (m_xFlipped)
                    alignment = Qt::AlignLeft;
                else
                    alignment = Qt::AlignRight;
                if (m_zFlipped)
                    rotation.setY(180.0f);
                else
                    rotation.setY(0.0f);

                labelTrans = QVector3D(-labelXTrans - labelMarginXTrans, labelPos,
                                       -labelZTrans + zComp);

                // Draw the label here
                m_dummyRenderItem.setTranslation(labelTrans);
                m_drawer->drawLabel(m_dummyRenderItem, axisLabelItem, viewMatrix, projectionMatrix,
                                    positionZComp, rotation, 0, m_cachedSelectionMode,
                                    m_labelShader, m_labelObj, m_cachedScene->activeCamera(),
                                    true, true, Drawer::LabelMid, alignment);
            }
            labelNbr++;
            labelPos += posStep;
        }
    }

    glDisable(GL_TEXTURE_2D);

    glDisable(GL_BLEND);

    // Release label shader
    m_labelShader->release();
}

void Surface3DRenderer::updateSurfaceGradient(const QLinearGradient &gradient)
{
    QImage image(QSize(4, 1000), QImage::Format_RGB32);
    QPainter pmp(&image);
    pmp.setBrush(QBrush(gradient));
    pmp.setPen(Qt::NoPen);
    pmp.drawRect(0, 0, 4, 1000);

    if (m_gradientTexture) {
        m_textureHelper->deleteTexture(&m_gradientTexture);
        m_gradientTexture = 0;
    }

    m_gradientTexture = m_textureHelper->create2DTexture(image, false, true);
}

void Surface3DRenderer::requestSelectionAtPoint(const QPoint &point)
{
    Q_UNUSED(point)

    //    QMutexLocker locker(&m_mutex);
    //    m_selectionPointRequest.setX(point.x());
    //    m_selectionPointRequest.setY(point.y());
    //    m_isSelectionPointRequestActive = true;

    m_querySelection = true;
}

// This one needs to be called when the data size changes
void Surface3DRenderer::updateSelectionTexture()
{
    // Create the selection ID image. Each grid corner gets 2x2 pixel area of
    // ID color so that each vertex (data point) has 4x4 pixel area of ID color
    // TODO: power of two thing for ES
    int idImageWidth = (m_sampleSpace.width() - 1) * 4;
    int idImageHeight = (m_sampleSpace.height() - 1) * 4;
    int stride = idImageWidth * 4 * sizeof(uchar); // 4 = number of color components (rgba)

    uchar *bits = new uchar[idImageWidth * idImageHeight * 4 * sizeof(uchar)];
    uint id = 1;
    for (int i = 0; i < idImageHeight; i += 4) {
        for (int j = 0; j < idImageWidth; j += 4) {
            int p = (i * idImageWidth + j) * 4;
            uchar r, g, b, a;
            idToRGBA(id, &r, &g, &b, &a);
            fillIdCorner(&bits[p], r, g, b, a, stride);

            idToRGBA(id + 1, &r, &g, &b, &a);
            fillIdCorner(&bits[p + 8], r, g, b, a, stride);

            idToRGBA(id + m_sampleSpace.width(), &r, &g, &b, &a);
            fillIdCorner(&bits[p + 2 * stride], r, g, b, a, stride);

            idToRGBA(id + m_sampleSpace.width() + 1, &r, &g, &b, &a);
            fillIdCorner(&bits[p + 2 * stride + 8], r, g, b, a, stride);

            id++;
        }
        id++;
    }

    // Use this to save the ID image to file
    //QImage image(bits, idImageWidth, idImageHeight, QImage::Format_ARGB32);
    //image.save("C:\\Users\\misalmel\\Work\\gerrit\\qtdatavisualization_2\\selection.png");

    // If old texture exists, delete it
    if (m_selectionTexture) {
        m_textureHelper->deleteTexture(&m_selectionTexture);
        m_selectionTexture = 0;
    }

    // Move the ID image (bits) to the texture
    m_selectionTexture = m_textureHelper->create2DTexture(bits, idImageWidth, idImageHeight);

    // Release the temp bits allocation
    delete[] bits;
}

void Surface3DRenderer::initSelectionBuffer()
{
    // Create the result selection texture and buffers
    if (m_selectionResultTexture) {
        m_textureHelper->deleteTexture(&m_selectionResultTexture);
        m_selectionResultTexture = 0;
    }

    m_selectionResultTexture = m_textureHelper->createSelectionTexture(m_mainViewPort.size(),
                                                                       m_selectionFrameBuffer,
                                                                       m_selectionDepthBuffer);
}

void Surface3DRenderer::fillIdCorner(uchar *p, uchar r, uchar g, uchar b, uchar a, int stride)
{
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = a;
    p[4] = r;
    p[5] = g;
    p[6] = b;
    p[7] = a;
    p[stride + 0] = r;
    p[stride + 1] = g;
    p[stride + 2] = b;
    p[stride + 3] = a;
    p[stride + 4] = r;
    p[stride + 5] = g;
    p[stride + 6] = b;
    p[stride + 7] = a;
}

void Surface3DRenderer::idToRGBA(uint id, uchar *r, uchar *g, uchar *b, uchar *a)
{
    *r = id & ID_TO_RGBA_MASK;
    *g = (id >> 8) & ID_TO_RGBA_MASK;
    *b = (id >> 16) & ID_TO_RGBA_MASK;
    *a = (id >> 24) & ID_TO_RGBA_MASK;
}

void Surface3DRenderer::updateTextures()
{
    qDebug() << __FUNCTION__ << "NEED TO DO SOMETHING";

    updateSurfaceGradient(m_cachedTheme.m_surfaceGradient);
}

void Surface3DRenderer::calculateSceneScalingFactors()
{
    // Calculate scene scaling and translation factors
    m_heightNormalizer = GLfloat(m_axisCacheY.max() - m_axisCacheY.min());
    m_areaSize.setHeight(m_axisCacheZ.max() -  m_axisCacheZ.min());
    m_areaSize.setWidth(m_axisCacheX.max() - m_axisCacheX.min());
    m_scaleFactor = qMax(m_areaSize.width(), m_areaSize.height());
#ifndef USE_UNIFORM_SCALING // Use this if we want to use autoscaling for x and z
    m_scaleX = aspectRatio * m_areaSize.width() / m_scaleFactor;
    m_scaleZ = aspectRatio * m_areaSize.height() / m_scaleFactor;
    m_scaleXWithBackground = m_scaleX * backgroundMargin;
    m_scaleZWithBackground = m_scaleZ * backgroundMargin;
#else // ..and this if we want uniform scaling based on largest dimension
    m_scaleX = aspectRatio / m_scaleFactor;
    m_scaleZ = aspectRatio / m_scaleFactor;
    m_scaleXWithBackground = aspectRatio * backgroundMargin;
    m_scaleZWithBackground = aspectRatio * backgroundMargin;
#endif
}

bool Surface3DRenderer::updateSmoothStatus(bool enable)
{
    m_cachedSmoothSurface = enable;
    if (!m_cachedSmoothSurface && !m_flatSupported) {
        qWarning() << "Warning: Flat qualifier not supported on your platform's GLSL language."
                      " Requires at least GLSL version 1.5.";
        m_cachedSmoothSurface = true;
    }

    if (!m_surfaceObj)
        return m_cachedSmoothSurface;

    if (m_cachedSmoothSurface == false && !m_flatSupported)
        m_cachedSmoothSurface = true;

    if (m_cachedSmoothSurface)
        m_surfaceObj->setUpSmoothData(m_dataArray, m_sampleSpace, m_heightNormalizer, true);
    else
        m_surfaceObj->setUpData(m_dataArray, m_sampleSpace, m_heightNormalizer, true);

    initSurfaceShaders();

    return m_cachedSmoothSurface;
}

void Surface3DRenderer::updateSurfaceGridStatus(bool enable)
{
    m_cachedSurfaceGridOn = enable;
}

void Surface3DRenderer::loadBackgroundMesh()
{
    if (m_backgroundObj)
        delete m_backgroundObj;
    m_backgroundObj = new ObjectHelper(QStringLiteral(":/defaultMeshes/background"));
    m_backgroundObj->load();
}

void Surface3DRenderer::loadSurfaceObj()
{
    if (m_surfaceObj)
        delete m_surfaceObj;
    m_surfaceObj = new SurfaceObject();
}

void Surface3DRenderer::loadGridLineMesh()
{
    if (m_gridLineObj)
        delete m_gridLineObj;
    m_gridLineObj = new ObjectHelper(QStringLiteral(":/defaultMeshes/bar"));
    m_gridLineObj->load();
}

void Surface3DRenderer::handleResize()
{
    if (m_cachedBoundingRect.width() == 0 || m_cachedBoundingRect.height() == 0)
        return;

    m_mainViewPort = QRect(0, 0, m_cachedBoundingRect.width(), m_cachedBoundingRect.height());

    if (m_selectionPointer)
        m_selectionPointer->updateBoundingRect(m_mainViewPort);

    Abstract3DRenderer::handleResize();
}

void Surface3DRenderer::surfacePointSelected(int id)
{
    int column = (id - 1) % m_sampleSpace.width();
    int row = (id - 1) / m_sampleSpace.width();
    qreal value = m_dataArray.at(row)->at(column);

    if (!m_selectionPointer)
        m_selectionPointer = new SelectionPointer(m_controller, m_drawer);

    QVector3D pos(normalize(float(column), value, float(row)));
    pos += QVector3D(m_surfaceOffsetX, 0.0f, m_surfaceOffsetZ - zComp);

    m_selectionPointer->setPosition(pos);
    m_selectionPointer->setScaling(QVector3D(m_surfaceScaleX, 1.0f, m_surfaceScaleZ));
    m_selectionPointer->setLabel(createSelectionLabel(value, column, row));
    m_selectionPointer->updateBoundingRect(m_mainViewPort);
    m_selectionPointer->updateScene(m_cachedScene);

    //Put the selection pointer flag active
    m_selectionActive = true;
}

QString Surface3DRenderer::createSelectionLabel(qreal value, int column, int row)
{
    QString labelText = itemLabelFormat();
    static const QString xTitleTag(QStringLiteral("@xTitle"));
    static const QString yTitleTag(QStringLiteral("@yTitle"));
    static const QString zTitleTag(QStringLiteral("@zTitle"));
    static const QString xLabelTag(QStringLiteral("@xLabel"));
    static const QString yLabelTag(QStringLiteral("@yLabel"));
    static const QString zLabelTag(QStringLiteral("@zLabel"));

    labelText.replace(xTitleTag, m_axisCacheX.title());
    labelText.replace(yTitleTag, m_axisCacheY.title());
    labelText.replace(zTitleTag, m_axisCacheZ.title());

    if (labelText.contains(xLabelTag)) {
        QString labelFormat = m_axisCacheX.labelFormat();
        if (labelFormat.isEmpty())
            labelFormat = Utils::defaultLabelFormat();
        QString valueLabelText = generateValueLabel(labelFormat, columnInRange(column));
        labelText.replace(xLabelTag, valueLabelText);
    }
    if (labelText.contains(yLabelTag)) {
        QString labelFormat = m_axisCacheY.labelFormat();
        if (labelFormat.isEmpty())
            labelFormat = Utils::defaultLabelFormat();
        QString valueLabelText = generateValueLabel(labelFormat, value);
        labelText.replace(yLabelTag, valueLabelText);
    }
    if (labelText.contains(zLabelTag)) {
        QString labelFormat = m_axisCacheZ.labelFormat();
        if (labelFormat.isEmpty())
            labelFormat = Utils::defaultLabelFormat();
        QString valueLabelText = generateValueLabel(labelFormat, rowInRange(row));
        labelText.replace(zLabelTag, valueLabelText);
    }

    return labelText;
}

// Transforms the model column coordinate to axis coordinate
qreal Surface3DRenderer::columnInRange(int column)
{
    return m_dataStepX * qreal(column) + m_minVisibleColumnValue;
}

// Transforms the model row coordinate to axis coordinate
qreal Surface3DRenderer::rowInRange(int row)
{
    return m_dataStepZ * qreal(row) + m_minVisibleRowValue;
}

QVector3D Surface3DRenderer::normalize(float x, float y, float z)
{
    float resX = x / ((float(m_sampleSpace.width()) - 1.0f) / 2.0f) - 1.0f;
    float resY = y / (m_heightNormalizer / 2.0f) - 1.0f;
    float resZ = z / ((float(m_sampleSpace.height()) - 1.0f) / -2.0f) + 1.0f;

    return QVector3D(resX, resY, resZ);
}

void Surface3DRenderer::surfacePointCleared()
{
    if (m_selectionPointer) {
        delete m_selectionPointer;
        m_selectionPointer = 0;
        m_selectionActive = false;
    }
}

void Surface3DRenderer::loadMeshFile()
{
    qDebug() << __FUNCTION__ << "should we do something";
}

void Surface3DRenderer::updateShadowQuality(QDataVis::ShadowQuality quality)
{
    qWarning() << "Shadows have been disabled for Q3DSurface in technology preview";
    m_cachedShadowQuality = QDataVis::ShadowNone; //quality;
    switch (quality) {
    case QDataVis::ShadowLow:
        m_shadowQualityToShader = 33.3f;
        m_shadowQualityMultiplier = 1;
        break;
    case QDataVis::ShadowMedium:
        m_shadowQualityToShader = 100.0f;
        m_shadowQualityMultiplier = 3;
        break;
    case QDataVis::ShadowHigh:
        m_shadowQualityToShader = 200.0f;
        m_shadowQualityMultiplier = 5;
        break;
    case QDataVis::ShadowSoftLow:
        m_shadowQualityToShader = 5.0f;
        m_shadowQualityMultiplier = 1;
        break;
    case QDataVis::ShadowSoftMedium:
        m_shadowQualityToShader = 10.0f;
        m_shadowQualityMultiplier = 3;
        break;
    case QDataVis::ShadowSoftHigh:
        m_shadowQualityToShader = 15.0f;
        m_shadowQualityMultiplier = 4;
        break;
    default:
        m_shadowQualityToShader = 0.0f;
        m_shadowQualityMultiplier = 1;
        break;
    }

#if !defined(QT_OPENGL_ES_2)
    updateDepthBuffer();
#endif
}

void Surface3DRenderer::loadLabelMesh()
{
    if (m_labelObj)
        delete m_labelObj;
    m_labelObj = new ObjectHelper(QStringLiteral(":/defaultMeshes/label"));
    m_labelObj->load();
}

void Surface3DRenderer::initShaders(const QString &vertexShader, const QString &fragmentShader)
{
    if (m_shader)
        delete m_shader;
    m_shader = new ShaderHelper(this, vertexShader, fragmentShader);
    m_shader->initialize();
}

void Surface3DRenderer::initBackgroundShaders(const QString &vertexShader,
                                              const QString &fragmentShader)
{
    if (m_backgroundShader)
        delete m_backgroundShader;
    m_backgroundShader = new ShaderHelper(this, vertexShader, fragmentShader);
    m_backgroundShader->initialize();
}

void Surface3DRenderer::initSelectionShaders()
{
    if (m_selectionShader)
        delete m_selectionShader;
    m_selectionShader = new ShaderHelper(this, QStringLiteral(":/shaders/vertexLabel"),
                                         QStringLiteral(":/shaders/fragmentLabel"));
    m_selectionShader->initialize();
}

void Surface3DRenderer::initSurfaceShaders()
{
    if (m_surfaceShader)
        delete m_surfaceShader;

#if !defined(QT_OPENGL_ES_2)
    if (m_cachedSmoothSurface) {
        m_surfaceShader = new ShaderHelper(this, QStringLiteral(":/shaders/vertexSurface"),
                                           QStringLiteral(":/shaders/fragmentSurface"));
    } else {
        m_surfaceShader = new ShaderHelper(this, QStringLiteral(":/shaders/vertexSurfaceFlat"),
                                           QStringLiteral(":/shaders/fragmentSurfaceFlat"));
    }
#else
    m_surfaceShader = new ShaderHelper(this, QStringLiteral(":/shaders/vertexSurface"),
                                       QStringLiteral(":/shaders/fragmentSurfaceES2"));
#endif
    m_surfaceShader->initialize();

    if (m_surfaceGridShader)
        delete m_surfaceGridShader;

    m_surfaceGridShader = new ShaderHelper(this, QStringLiteral(":/shaders/vertexSurfaceGrid"),
                                           QStringLiteral(":/shaders/fragmentSurfaceGrid"));

    m_surfaceGridShader->initialize();
}

void Surface3DRenderer::initLabelShaders(const QString &vertexShader, const QString &fragmentShader)
{
    if (m_labelShader)
        delete m_labelShader;
    m_labelShader = new ShaderHelper(this, vertexShader, fragmentShader);
    m_labelShader->initialize();
}

#if !defined(QT_OPENGL_ES_2)
void Surface3DRenderer::initDepthShader()
{
    // TODO: Implement a depth shader for surface after technology preview
    if (m_depthShader)
        delete m_depthShader;
    m_depthShader = new ShaderHelper(this, QStringLiteral(":/shaders/vertexDepth"),
                                     QStringLiteral(":/shaders/fragmentDepth"));
    m_depthShader->initialize();
}

void Surface3DRenderer::updateDepthBuffer()
{
    if (m_depthTexture) {
        m_textureHelper->deleteTexture(&m_depthTexture);
        m_depthTexture = 0;
    }

    if (m_cachedShadowQuality > QDataVis::ShadowNone) {
        m_depthTexture = m_textureHelper->createDepthTexture(m_mainViewPort.size(),
                                                             m_depthFrameBuffer,
                                                             m_shadowQualityMultiplier);
        if (!m_depthTexture) {
            switch (m_cachedShadowQuality) {
            case QDataVis::ShadowHigh:
                qWarning("Creating high quality shadows failed. Changing to medium quality.");
                (void)m_controller->setShadowQuality(QDataVis::ShadowMedium);
                updateShadowQuality(QDataVis::ShadowMedium);
                break;
            case QDataVis::ShadowMedium:
                qWarning("Creating medium quality shadows failed. Changing to low quality.");
                (void)m_controller->setShadowQuality(QDataVis::ShadowLow);
                updateShadowQuality(QDataVis::ShadowLow);
                break;
            case QDataVis::ShadowLow:
                qWarning("Creating low quality shadows failed. Switching shadows off.");
                (void)m_controller->setShadowQuality(QDataVis::ShadowNone);
                updateShadowQuality(QDataVis::ShadowNone);
                break;
            case QDataVis::ShadowSoftHigh:
                qWarning("Creating soft high quality shadows failed. Changing to soft medium quality.");
                (void)m_controller->setShadowQuality(QDataVis::ShadowSoftMedium);
                updateShadowQuality(QDataVis::ShadowSoftMedium);
                break;
            case QDataVis::ShadowSoftMedium:
                qWarning("Creating soft medium quality shadows failed. Changing to soft low quality.");
                (void)m_controller->setShadowQuality(QDataVis::ShadowSoftLow);
                updateShadowQuality(QDataVis::ShadowSoftLow);
                break;
            case QDataVis::ShadowSoftLow:
                qWarning("Creating soft low quality shadows failed. Switching shadows off.");
                (void)m_controller->setShadowQuality(QDataVis::ShadowNone);
                updateShadowQuality(QDataVis::ShadowNone);
                break;
            default:
                // You'll never get here
                break;
            }
        }
    }
}
#endif

QT_DATAVISUALIZATION_END_NAMESPACE
