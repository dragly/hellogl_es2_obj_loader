#include "model.h"

Model::Model() {
}

Model::Model(QString objectFile) {
    load(objectFile);
}
void Model::load(QString filename) {
    model = glmReadOBJ(filename.toLatin1().data());
    if(model->numtexcoords < 1) {
        qWarning() << "Missing UV map.";
    }
    GLMgroup* group;
    group = model->groups;
    while (group) {
        ModelGroup grp;
        for(int i = 0; i < group->numtriangles; i++) {
            ModelTriangle triangle;
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
    qDebug() << "loading file";
}
bool Model::setFragmentShaderFile(QString filename) {
    if(!program.addShaderFromSourceFile(QGLShader::Fragment, filename)) {
        qDebug() << "Could not load shader file " + filename + ": " << program.log();
        return false;
    } else {
        qDebug() << "Loaded " + filename + " successfully";
        return true;
    }
}
bool Model::setVertexShaderFile(QString filename) {
    if(!program.addShaderFromSourceFile(QGLShader::Vertex, filename)) {
        qDebug() << "Could not load shader file " + filename + ": " << program.log();
        return false;
    } else {
        qDebug() << "Loaded " + filename + " successfully";
        return true;
    }
}
bool Model::linkShaderProgram() {
    if(program.link()) {
        qDebug() << "Program linked";
        return true;
    } else {
        qDebug() << "Failed to link program:" << program.log();
        return false;
    }
}

void Model::initShaderProgram() {
    vertexAttr = program.attributeLocation("vertex");
    normalAttr = program.attributeLocation("normal");
    texCoordAttr = program.attributeLocation("texCoord");
    matrixUniform = program.uniformLocation("matrix");
    textureUniform = program.uniformLocation("tex");
}

void Model::draw(QMatrix4x4 modelview) {
    program.bind();
    glBindTexture(GL_TEXTURE_2D, texture);
    foreach(ModelGroup grp, groups) {
        foreach(ModelTriangle triangle, grp.triangles) {
            program.setUniformValue(textureUniform, 0);    // use texture unit 0
            program.enableAttributeArray(normalAttr);
            program.enableAttributeArray(vertexAttr);
            program.enableAttributeArray(texCoordAttr);
            program.setAttributeArray(vertexAttr, triangle.vertices.constData());
            program.setAttributeArray(normalAttr, triangle.normals.constData());
            program.setAttributeArray(texCoordAttr, triangle.texcoords.constData());
            glDrawArrays(GL_TRIANGLES, 0, triangle.vertices.size());
            program.disableAttributeArray(normalAttr);
            program.disableAttributeArray(vertexAttr);
            program.disableAttributeArray(texCoordAttr);
        }
    }
    program.setUniformValue(matrixUniform, modelview);
    program.release();
    program.bind();
}
void Model::setTexture(GLuint texture) {
    this->texture = texture;
}
