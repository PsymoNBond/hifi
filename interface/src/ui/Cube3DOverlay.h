//
//  Cube3DOverlay.h
//  interface
//
//  Copyright (c) 2014 High Fidelity, Inc. All rights reserved.
//

#ifndef __interface__Cube3DOverlay__
#define __interface__Cube3DOverlay__

// include this before QGLWidget, which includes an earlier version of OpenGL
#include "InterfaceConfig.h"

#include <QGLWidget>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRect>
#include <QScriptValue>
#include <QString>
#include <QUrl>

#include <SharedUtil.h>

#include "Base3DOverlay.h"

class Cube3DOverlay : public Base3DOverlay {
    Q_OBJECT
    
public:
    Cube3DOverlay();
    ~Cube3DOverlay();
    virtual void render();
};

 
#endif /* defined(__interface__Cube3DOverlay__) */
