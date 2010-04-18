/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "glwidget.h"
#include <QPainter>
#include <QPaintEngine>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bubble.h"

const qreal max_momentum = 40.0;
const qreal momentum_slowdown = 0.5;
const int bubbleNum = 8;

GLWidget::GLWidget(QWidget *parent)
    : QGLWidget(parent)
{
    qtLogo = true;
    frames = 0;
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoBufferSwap(false);
    model = glmReadOBJ("monkey1.obj");
    if(model->numtexcoords < 1) {
        qWarning() << "Missing UV map.";
    }
    GLMgroup* group;
    group = model->groups;
    while (group) {
        Group grp;
        for(int i = 0; i < group->numtriangles; i++) {
            Triangle triangle;
            QVector<QVector3D> verts;
            for(int j = 0; j < 3; j++) {
                QVector3D vector(model->vertices[3 * model->triangles[group->triangles[i]].vindices[j] + 0],
                                 model->vertices[3 * model->triangles[group->triangles[i]].vindices[j] + 1],
                                 model->vertices[3 * model->triangles[group->triangles[i]].vindices[j] + 2]);
                verts.append(vector);
            }
            QVector<QVector3D> norms;
            for(int j = 0; j < 3; j++) {
                QVector3D vector(model->normals[3 * model->triangles[group->triangles[i]].nindices[j] + 0],
                                 model->normals[3 * model->triangles[group->triangles[i]].nindices[j] + 1],
                                 model->normals[3 * model->triangles[group->triangles[i]].nindices[j] + 2]);
                norms.append(vector);
            }
            if(model->numtexcoords > 0) {
                QVector<QVector3D> texs;
                for(int j = 0; j < 3; j++) {
                    QVector3D vector(model->texcoords[2 * model->triangles[group->triangles[i]].tindices[j] + 0],
                                     model->texcoords[2 * model->triangles[group->triangles[i]].tindices[j] + 1],
                                     model->texcoords[2 * model->triangles[group->triangles[i]].tindices[j] + 2]);
                    texs.append(vector);
                }
                triangle.texcoords = texs;
            }
            triangle.vertices = verts;
            triangle.normals = norms;
            grp.triangles.append(triangle);
        }
        groups.append(grp);
        group = group->next;
    }
    // initial values
    rotation.setX(0);
    rotation.setY(0);
    rotation.setZ(0);
    camera = QVector3D(-5, -3, 20);
    // timer
    timer = new QTimer(this);
    QObject::connect(timer, SIGNAL(timeout()), this, SLOT(updateGL()));
    timer->setInterval(1);
    timer->start();
}

GLWidget::~GLWidget()
{
}

void GLWidget::resizeGL(int width, int height) {
    aspectRatio = (qreal) width / (qreal) height;
}

void GLWidget::initializeGL ()
{
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glGenTextures(1, &m_uiTexture);
    m_uiTexture = bindTexture(QImage(":/fur.resized.jpg"));
    QFile vfile("vshader.glsl");
    if(!vfile.open(QIODevice::ReadOnly)) return;
    QString vsrc1 = vfile.readAll();
    QGLShader *vshader1 = new QGLShader(QGLShader::Vertex, this);
    vshader1->compileSourceCode(vsrc1.toAscii());
    vfile.close();

    QFile ffile("fshader.glsl");
    if(!ffile.open(QIODevice::ReadOnly)) return;
    QString fsrc1 = ffile.readAll();
    qDebug() << "Output:\n" << fsrc1;
    QGLShader *fshader1 = new QGLShader(QGLShader::Fragment, this);
    fshader1->compileSourceCode(fsrc1.toAscii());
    ffile.close();

    program1.addShader(vshader1);
    program1.addShader(fshader1);
    program1.link();

    vertexAttr1 = program1.attributeLocation("vertex");
    normalAttr1 = program1.attributeLocation("normal");
    texCoordAttr1 = program1.attributeLocation("texCoord");
    matrixUniform1 = program1.uniformLocation("matrix");
    textureUniform1 = program1.uniformLocation("tex");

    QGLShader *vshader2 = new QGLShader(QGLShader::Vertex);
    const char *vsrc2 =
            "attribute highp vec4 vertex;\n"
            "attribute highp vec4 texCoord;\n"
            "attribute mediump vec3 normal;\n"
            "uniform mediump mat4 matrix;\n"
            "varying highp vec4 texc;\n"
            "varying mediump float angle;\n"
            "void main(void)\n"
            "{\n"
            "    vec3 toLight = normalize(vec3(0.0, 0.3, 1.0));\n"
            "    angle = max(dot(normal, toLight), 0.0);\n"
            "    gl_Position = matrix * vertex;\n"
            "    texc = texCoord;\n"
            "}\n";
    vshader2->compileSourceCode(vsrc2);

    QGLShader *fshader2 = new QGLShader(QGLShader::Fragment);
    const char *fsrc2 =
            "varying highp vec4 texc;\n"
            "uniform sampler2D tex;\n"
            "varying mediump float angle;\n"
            "void main(void)\n"
            "{\n"
            "    highp vec3 color = texture2D(tex, texc.st).rgb;\n"
            "    color = color * 0.2 + color * 0.8 * angle;\n"
            "    gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);\n"
            "}\n";
    fshader2->compileSourceCode(fsrc2);

    program2.addShader(vshader2);
    program2.addShader(fshader2);
    program2.link();

    vertexAttr2 = program2.attributeLocation("vertex");
    normalAttr2 = program2.attributeLocation("normal");
    texCoordAttr2 = program2.attributeLocation("texCoord");
    matrixUniform2 = program2.uniformLocation("matrix");
    textureUniform2 = program2.uniformLocation("tex");

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
}

void GLWidget::paintMonkey()
{
    glBindTexture(GL_TEXTURE_2D, m_uiTexture);
    foreach(Group grp, groups) {
        foreach(Triangle triangle, grp.triangles) {
            program1.setUniformValue(textureUniform1, 0);    // use texture unit 0
            program1.enableAttributeArray(normalAttr1);
            program1.enableAttributeArray(vertexAttr1);
            program1.enableAttributeArray(texCoordAttr1);
            program1.setAttributeArray(vertexAttr1, triangle.vertices.constData());
            program1.setAttributeArray(normalAttr1, triangle.normals.constData());
            program1.setAttributeArray(texCoordAttr1, triangle.texcoords.constData());
            glDrawArrays(GL_TRIANGLES, 0, triangle.vertices.size());
            program1.disableAttributeArray(normalAttr1);
            program1.disableAttributeArray(vertexAttr1);
            program1.disableAttributeArray(texCoordAttr1);
        }
    }
}
void GLWidget::paintGL()
{
    // do rotation - could we do this is one loop for xyz?
    if (momentum.z() > max_momentum) {
        momentum.setZ(max_momentum);
    } else if (momentum.z() < -max_momentum) {
        momentum.setZ(-max_momentum);
    }
    if(momentum.z() > 0) {
        momentum -= QVector3D(0, 0, momentum_slowdown * (qreal) time.elapsed() / 1000.0);
        if(momentum.z() < 0) {
            momentum.setZ(0);
        }
    }
    else if(momentum.z() < 0) {
        momentum += QVector3D(0, 0, momentum_slowdown * (qreal) time.elapsed() / 1000.0);
        if(momentum.z() > 0) {
            momentum.setZ(0);
        }
    }
    rotation += QVector3D(0,0,momentum.z() * 0.1);
    if (momentum.x() > max_momentum) {
        momentum.setX(max_momentum);
    } else if (momentum.x() < -max_momentum) {
        momentum.setX(-max_momentum);
    }
    if(momentum.x() > 0) {
        momentum -= QVector3D(momentum_slowdown * (qreal) time.elapsed() / 1000.0, 0, 0);
        if(momentum.x() < 0) {
            momentum.setX(0);
        }
    }
    else if(momentum.x() < 0) {
        momentum += QVector3D(momentum_slowdown * (qreal) time.elapsed() / 1000.0, 0, 0);
        if(momentum.x() > 0) {
            momentum.setX(0);
        }
    }
    rotation += QVector3D(momentum.x() * 0.1, 0,0);
    // end rotation

    //    createBubbles(bubbleNum - bubbles.count());

    QPainter painter;
    painter.begin(this);

    painter.beginNativePainting();

    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    //    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

    glFrontFace(GL_CW);
    glCullFace(GL_FRONT);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    //    glDepthMask(GL_TRUE);
    mainModelView = QMatrix4x4(); // reset
    // set up the main view (affects all objects)
    mainModelView.perspective(60.0, aspectRatio, 1.0, 50.0);
    mainModelView.lookAt(camera,QVector3D(0,0,0),QVector3D(0.0,0.0,1.0));
    //    mainModelView.rotate(rotation.z(), 0.0, 0.0, 1.0);
    // inherit the main view for each object
    QMatrix4x4 mvMonkey = mainModelView;
    QMatrix4x4 mvMonkey2 = mainModelView;
    QMatrix4x4 mvMonkey3 = mainModelView;
    QMatrix4x4 mvMonkey4 = mainModelView;
    QMatrix4x4 mvMonkey5 = mainModelView;
    //    mvMonkey5.rotate(rotation.x(), 1.0, 0.0, 0.0);
    //    mvMonkey5.rotate(rotation.y(), 0.0, 1.0, 0.0);
    // do whatever with each object
    //    mvMonkey.rotate(90, 1, 0, 0);
    //    mvMonkey.scale(m_fScale * 2.0);
    mvMonkey.translate(-2.0,0,0);
    mvMonkey3.translate(2.0,0,0);
    mvMonkey4.translate(4.0,0,0);
    mvMonkey5.translate(player.x(),player.y(),player.z());
    mvMonkey5.rotate(rotation.z(), 0.0, 0.0, 1.0);
    program1.bind();
    paintMonkey();
    program1.setUniformValue(matrixUniform1, mvMonkey);
    program1.release();
    program1.bind();
    paintMonkey();
    program1.setUniformValue(matrixUniform1, mvMonkey2);
    program1.release();
    program1.bind();
    paintMonkey();
    program1.setUniformValue(matrixUniform1, mvMonkey3);
    program1.release();
    program1.bind();
    paintMonkey();
    program1.setUniformValue(matrixUniform1, mvMonkey4);
    program1.release();
    program1.bind();
    paintMonkey();
    program1.setUniformValue(matrixUniform1, mvMonkey5);
    program1.release();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    painter.endNativePainting();

    QString framesPerSecond;
    framesPerSecond.setNum(frames /(time.elapsed() / 1000.0), 'f', 2);

    painter.setPen(Qt::white);

    painter.drawText(20, 40, framesPerSecond + " fps");
    painter.drawText(20, 50, "momentum: " + QString::number(momentum.x()) + ", " + QString::number(momentum.y()) + ", " + QString::number(momentum.z()));
    painter.drawText(20, 60, "pos: " + QString::number(player.x()) + ", " + QString::number(player.y()) + ", " + QString::number(player.z()));

    painter.end();

    swapBuffers();

    if (!(frames % 100)) {
        time.start();
        frames = 0;
    }
    frames ++;
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragStartPosition = event->pos();
        dragtime.start();

        // project click down to plane
        // Another attempt
        // mainModelView should be our modelview projection matrix
        QMatrix4x4 inv = mainModelView.inverted();
        qDebug() << "inv" << inv;
        qreal coordx = (qreal) event->x() / (qreal) width();
        qreal coordy = (qreal) (height() - event->y()) / (qreal) height();
        // alright, don't ask me why, but we need to do this get the right position on screen
        coordx *= -2.0;
        coordx += 1.0;
        coordy *= -2.0;
        coordy += 1.0;
        // end workaround - one day we should fix this :)
        QVector3D screen = inv * QVector3D(coordx,coordy,-1);
        qDebug() << "screen" << screen;
        QVector3D center = inv * QVector3D(0, 0, 0);
        qDebug() << "center" << center;
        QVector3D dir = center - screen;
        qDebug() << "direction" << dir;
        // line is r = camera + t * dir

        if (dir.z()==0.0) // if we are looking in a flat direction
            return;

        qreal t = - (camera.z()) / dir.z(); // how long it is to the ground
        qDebug() << "t" << t;
        player.setX(camera.x() + dir.x() * t);
        player.setY(camera.y() + dir.y() * t);
        player.setZ(camera.z() + dir.z() * t); // should become zero
    }
}
// Dragging events
void GLWidget::mouseMoveEvent(QMouseEvent* event) {
    if(!(event->buttons() & Qt::LeftButton))
        return;
    if(dragging) {
        int elapsed = dragtime.elapsed();
        QVector3D oldvalue = momentum;
        qreal relativey = (dragLastPosition.y() - event->pos().y()) / (qreal) height();
        qreal relativex = (event->pos().x() - dragLastPosition.x()) / (qreal) width();
        relativey *= 2500; // increase the factor
        relativex *= 2500;
        QVector3D vector = QVector3D(relativey / (qreal) elapsed, 0, relativex / (qreal) elapsed);
        momentum += vector;
        if (isinf(momentum.x()) || isnan(momentum.x())) {
            momentum.setX(oldvalue.x());
        }
        if (isinf(momentum.y()) || isnan(momentum.y())) {
            momentum.setY(oldvalue.y());
        }
        if (isinf(momentum.z()) || isnan(momentum.z())) {
            momentum.setZ(oldvalue.z());
        }
    }
    dragLastPosition = event->pos();
    dragging = true;
    dragtime.restart();
}
void GLWidget::mouseReleaseEvent(QMouseEvent *event) {
    dragging = false;

}
