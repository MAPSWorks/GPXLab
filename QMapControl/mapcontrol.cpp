/*
*
* This file is part of QMapControl,
* an open-source cross-platform map widget
*
* Copyright (C) 2007 - 2008 Kai Winter
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with QMapControl. If not, see <http://www.gnu.org/licenses/>.
*
* Contact e-mail: kaiwinter@gmx.de
* Program URL   : http://qmapcontrol.sourceforge.net/
*
*/

#include "mapcontrol.h"
#include <QTimer>

namespace qmapcontrol
{

    MapControl::MapControl (QWidget * parent, Qt::WindowFlags windowFlags)
        :   QWidget( parent, windowFlags ),
            size(100,100),
            mymousemode(Panning),
            scaleVisible(false),
            crosshairsVisible(true),
            updateSuspendedCounter(0)
    {
        __init();
    }

    MapControl::MapControl(QSize size, MouseMode mousemode, bool showScale, bool showCrosshairs, QWidget * parent, Qt::WindowFlags windowFlags)
        :   QWidget( parent, windowFlags ),
            size(size),
            mymousemode(mousemode),
            scaleVisible(showScale),
            crosshairsVisible(showCrosshairs),
            updateSuspendedCounter(0)
    {
        __init();
    }

    MapControl::~MapControl()
    {
        if ( layermanager )
        {
            delete layermanager;
        }
    }

    void MapControl::__init()
    {
        layermanager = new LayerManager(this, size);
        screen_middle = QPoint(size.width()/2, size.height()/2);

        mousepressed = false;

        connect(ImageManager::instance(), SIGNAL(imageReceived()),
                this, SLOT(updateRequestNew()));

        connect(ImageManager::instance(), SIGNAL(loadingFinished()),
                this, SLOT(loadingFinished()));

        this->setMaximumSize(size.width()+1, size.height()+1);
        mouse_wheel_events = true;

        // enable mouse move events also if no mouse button is clicked
        setMouseTracking(true);
    }

    void MapControl::enableMouseWheelEvents( bool enabled )
    {
        mouse_wheel_events = enabled;
    }

    bool MapControl::mouseWheelEventsEnabled()
    {
        return mouse_wheel_events;
    }

    QPointF MapControl::currentCoordinate() const
    {
        return layermanager->currentCoordinate();
    }

    Layer* MapControl::layer(const QString& layername) const
    {
        return layermanager->layer(layername);
    }

    QList<QString> MapControl::layers() const
    {
        return layermanager->layers();
    }

    int MapControl::numberOfLayers() const
    {
        return layermanager->layers().size();
    }

    void MapControl::followGeometry(const Geometry* geom) const
    {
        if ( geom == 0 )
        {
            return;
        }

        //ensures only one signal is ever connected
        stopFollowing(geom);

        connect(geom, SIGNAL(positionChanged(Geometry*)),
                this, SLOT(positionChanged(Geometry*)));
    }

    void MapControl::positionChanged(Geometry* geom)
    {
        if ( !layermanager->layer() || !layermanager->layer()->mapadapter() )
        {
            qDebug() << "MapControl::positionChanged() - no layers configured";
            return;
        }

        Point* point = dynamic_cast<Point*>(geom);
        if (point!=0)
        {
            QPoint start = layermanager->layer()->mapadapter()->coordinateToDisplay(currentCoordinate());
            QPoint dest = layermanager->layer()->mapadapter()->coordinateToDisplay(point->coordinate());
            QPoint step = (dest-start);
            layermanager->scrollView(step);
            updateRequestNew();
        }
    }

    void MapControl::moveTo(QPointF coordinate)
    {
        target = coordinate;
        steps = 25;
        if (moveMutex.tryLock())
        {
            QTimer::singleShot(40, this, SLOT(tick()));
        }
        else
        {
            // stopMove(coordinate);
            moveMutex.unlock();
        }
    }
    void MapControl::tick()
    {
        if ( !layermanager->layer() || !layermanager->layer()->mapadapter() )
        {
            qDebug() << "MapControl::tick() - no layers configured";
            return;
        }

        QPoint start = layermanager->layer()->mapadapter()->coordinateToDisplay(currentCoordinate());
        QPoint dest = layermanager->layer()->mapadapter()->coordinateToDisplay(target);

        QPoint step = (dest-start)/steps;
        layermanager->scrollView(step);

        update();
        layermanager->updateRequest();
        steps--;
        if (steps>0)
        {
            QTimer::singleShot(50, this, SLOT(tick()));
        }
        else
        {
            moveMutex.unlock();
        }
    }

    void MapControl::paintEvent(QPaintEvent* evnt)
    {
        Q_UNUSED(evnt);

        static QPixmap *doubleBuffer( new QPixmap(width(), height()) );

        //check for resize change
        if ( doubleBuffer->width() != width() || doubleBuffer->height() != height() )
        {
            delete doubleBuffer;
            doubleBuffer = new QPixmap(width(), height());
        }

        QPainter dbPainter;
        dbPainter.begin(doubleBuffer);

        layermanager->drawImage(&dbPainter);
        layermanager->drawGeoms(&dbPainter);

        // draw scale
        if (scaleVisible)
        {
            static QList<double> distanceList;
            if (distanceList.isEmpty())
            {
                distanceList<<5000000<<2000000<<1000000<<1000000<<1000000<<100000<<100000<<50000<<50000<<10000<<10000<<10000<<1000<<1000<<500<<200<<100<<50<<25;
            }

            if (currentZoom() >= layermanager->minZoom() && distanceList.size() > currentZoom())
            {
                double line;
                line = distanceList.at( currentZoom() ) / pow(2.0, 18-currentZoom() ) / 0.597164;

                // draw the scale
                dbPainter.setPen(Qt::black);
                QPoint p1(10,size.height()-20);
                QPoint p2((int)line,size.height()-20);
                dbPainter.drawLine(p1,p2);

                dbPainter.drawLine(10,size.height()-15, 10,size.height()-25);
                dbPainter.drawLine((int)line,size.height()-15, (int)line,size.height()-25);

                QString distance;
                if (distanceList.at(currentZoom()) >= 1000)
                {
                    distance = QVariant( distanceList.at(currentZoom())/1000 )  .toString()+ " km";
                }
                else
                {
                    distance = QVariant( distanceList.at(currentZoom()) ).toString() + " m";
                }

                dbPainter.drawText(QPoint((int)line+10,size.height()-15), distance);
            }
        }

        if (crosshairsVisible)
        {
            dbPainter.drawLine(screen_middle.x(), screen_middle.y()-10,
                             screen_middle.x(), screen_middle.y()+10); // |
            dbPainter.drawLine(screen_middle.x()-10, screen_middle.y(),
                             screen_middle.x()+10, screen_middle.y()); // -
        }

        dbPainter.drawRect(0,0, size.width(), size.height());

        if (mousepressed && (mymousemode == Dragging || mymousemode == DraggingNoZoom))
        {
            QRect rect = QRect(pre_click_px, current_mouse_pos);
            dbPainter.drawRect(rect);
        }
        dbPainter.end();
        QPainter painter;
        painter.begin( this );
        painter.drawPixmap( rect(), *doubleBuffer, doubleBuffer->rect() );
        painter.end();
    }

    bool MapControl::isMousePressed() const
    {
        return mousepressed;
    }

    // mouse events
    void MapControl::mousePressEvent(QMouseEvent* evnt)
    {
        if (evnt->button() == 1)
        {
            mousepressed = true;
        }

        if (!(evnt->modifiers() & Qt::ShiftModifier))
        {
            layermanager->mouseEvent(evnt);

            if (layermanager->layers().size()>0)
            {
                if (evnt->button() == 1)
                {
                    mousepressed = true;
                    pre_click_px = QPoint(evnt->x(), evnt->y());
                }
                else if ( evnt->button() == 2  &&
                          mouseWheelEventsEnabled() &&
                          (mymousemode == Panning || mymousemode == Dragging)) // zoom in
                {
                    zoomIn();
                }
                else if  ( evnt->button() == 4 &&
                             mouseWheelEventsEnabled() &&
                             (mymousemode == Panning || mymousemode == Dragging)) // zoom out
                {
                    zoomOut();
                }
            }
        }

        // emit(mouseEvent(evnt));
        emit(mouseEventCoordinate(evnt, clickToWorldCoordinate(evnt->pos())));
    }

    void MapControl::mouseReleaseEvent(QMouseEvent* evnt)
    {
        mousepressed = false;
        if (mymousemode == Dragging || mymousemode == DraggingNoZoom)
        {
            QPointF ulCoord = clickToWorldCoordinate(pre_click_px);
            QPointF lrCoord = clickToWorldCoordinate(current_mouse_pos);

            QRectF coordinateBB = QRectF(ulCoord, QSizeF( (lrCoord-ulCoord).x(), (lrCoord-ulCoord).y()));

            emit(boxDragged(coordinateBB));
        }

        emit(mouseEventCoordinate(evnt, clickToWorldCoordinate(evnt->pos())));
    }

    void MapControl::mouseMoveEvent(QMouseEvent* evnt)
    {
        if (mousepressed && (mymousemode == Panning || mymousemode == PanningNoZoom))
        {
            QPoint offset = pre_click_px - QPoint(evnt->x(), evnt->y());
            layermanager->scrollView(offset);
            pre_click_px = QPoint(evnt->x(), evnt->y());
        }
        else if (mousepressed && (mymousemode == Dragging || mymousemode == DraggingNoZoom))
        {
            current_mouse_pos = QPoint(evnt->x(), evnt->y());
        }
        else
        {
            layermanager->mouseMoveEvent(evnt);
        }

        update();
    }

    void MapControl::wheelEvent(QWheelEvent *evnt)
    {
        if(mouse_wheel_events &&
            evnt->orientation() == Qt::Vertical)
        {
            if(evnt->delta() > 0)
            {
                if( currentZoom() == layermanager->maxZoom() )
                {
                    return;
                }
                suspendUpdate(true);
                setView(clickToWorldCoordinate(evnt->pos())); //zoom in under mouse cursor
                zoomIn();
                suspendUpdate(false);
                layermanager->forceRedraw();
            }
            else
            {
                if( currentZoom() == layermanager->minZoom() )
                {
                    return;
                }
                suspendUpdate(true);
                zoomOut();
                suspendUpdate(false);
                layermanager->forceRedraw();
            }
            evnt->accept();
        }
        else
        {
            evnt->ignore();
        }
    }

    QPointF MapControl::clickToWorldCoordinate(QPoint click)
    {
        if ( !layermanager->layer() || !layermanager->layer()->mapadapter() )
        {
            qDebug() << "MapControl::clickToWorldCoordinate() - no layers configured";
            return QPointF();
        }
        // click coordinate to image coordinate
        QPoint displayToImage= QPoint(click.x()-screen_middle.x()+layermanager->getMapmiddle_px().x(),
                                      click.y()-screen_middle.y()+layermanager->getMapmiddle_px().y());

        // image coordinate to world coordinate
        return layermanager->layer()->mapadapter()->displayToCoordinate(displayToImage);
    }

    void MapControl::updateRequest(QRect rect)
    {
        update(rect);
    }

    void MapControl::updateRequestNew()
    {
        layermanager->forceRedraw();
    }

    // slots
    void MapControl::zoomIn()
    {
        layermanager->zoomIn();
        updateView();
        emit viewChanged(currentCoordinate(), currentZoom());
    }

    void MapControl::zoomOut()
    {
        layermanager->zoomOut();
        updateView();
        emit viewChanged(currentCoordinate(), currentZoom());
    }

    void MapControl::setZoom(int zoomlevel)
    {
        layermanager->setZoom(zoomlevel);
        updateView();
        emit viewChanged(currentCoordinate(), currentZoom());
    }

    int MapControl::currentZoom() const
    {
        return layermanager->currentZoom();
    }

    int MapControl::minZoom() const
    {
        return layermanager->minZoom();
    }

    int MapControl::maxZoom() const
    {
        return layermanager->maxZoom();
    }

    void MapControl::scrollLeft(int pixel)
    {
        layermanager->scrollView(QPoint(-pixel,0));
        updateView();
    }

    void MapControl::scrollRight(int pixel)
    {
        layermanager->scrollView(QPoint(pixel,0));
        updateView();
    }

    void MapControl::scrollUp(int pixel)
    {
        layermanager->scrollView(QPoint(0,-pixel));
        updateView();
    }

    void MapControl::scrollDown(int pixel)
    {
        layermanager->scrollView(QPoint(0,pixel));
        updateView();
    }

    void MapControl::scroll(const QPoint scroll)
    {
        layermanager->scrollView(scroll);
        updateView();
    }

    void MapControl::updateView() const
    {
        layermanager->setView( currentCoordinate() );
        emit viewChanged(currentCoordinate(), currentZoom());
    }

    void MapControl::setView(const QPointF& coordinate) const
    {
        layermanager->setView(coordinate);
        emit viewChanged(currentCoordinate(), currentZoom());
    }

    void MapControl::setView(const QList<QPointF> coordinates) const
    {
        layermanager->setView(coordinates);
        emit viewChanged(currentCoordinate(), currentZoom());
    }

    void MapControl::setViewAndZoomIn(const QList<QPointF> coordinates) const
    {
        layermanager->setViewAndZoomIn(coordinates);
        emit viewChanged(currentCoordinate(), currentZoom());
    }

    void MapControl::setView(const Point* point) const
    {
        layermanager->setView(point->coordinate());
    }

    void MapControl::loadingFinished()
    {
        layermanager->removeZoomImage();
    }

    void MapControl::addLayer(Layer* layer)
    {
        layermanager->addLayer(layer);
        update();
    }

    void MapControl::removeLayer( Layer* layer )
    {
        disconnect(layer, 0, 0, 0);
        layermanager->removeLayer( layer );
        update();
    }

    void MapControl::setMouseMode(MouseMode mousemode)
    {
        mymousemode = mousemode;
    }

    MapControl::MouseMode MapControl::mouseMode()
    {
        return mymousemode;
    }

    void MapControl::stopFollowing(const Geometry* geom) const
    {
        disconnect(geom,SIGNAL(positionChanged(Geometry*)), this, SLOT(positionChanged(Geometry*)));
    }

    void MapControl::enablePersistentCache( const QDir& path, const int qDiskSizeMB )
    {
        ImageManager::instance()->setCacheDir( path, qDiskSizeMB );
    }

    void MapControl::setProxy(QString host, int port, const QString username, const QString password)
    {
        ImageManager::instance()->setProxy(host, port, username, password);
    }

    void MapControl::showScale(bool visible)
    {
        scaleVisible = visible;
    }

    void MapControl::showCrosshairs(bool visible)
    {
        crosshairsVisible = visible;
    }

    void MapControl::resize(const QSize newSize)
    {
        if (newSize != this->size)
        {
            this->size = newSize;
            screen_middle = QPoint(newSize.width()/2, newSize.height()/2);

            this->setMaximumSize(newSize.width()+1, newSize.height()+1);
            layermanager->resize(newSize);

            emit viewChanged(currentCoordinate(), currentZoom());
        }
    }

   void MapControl::setUseBoundingBox( bool usebounds )
   {
        if( layermanager )
            layermanager->setUseBoundingBox( usebounds );
   }

   bool MapControl::isBoundingBoxEnabled()
   {
        if( layermanager )
            return layermanager->isBoundingBoxEnabled();
        return false;
   }

   void MapControl::setBoundingBox( QRectF &rect )
   {
        if( layermanager )
            layermanager->setBoundingBox( rect );
   }

   QRectF MapControl::getBoundingBox()
   {
        if( layermanager )
            return layermanager->getBoundingBox();

        // Return an empty QRectF if there is no layermanager
        return QRectF();
   }

   QRectF MapControl::getViewport()
   {
       if( layermanager )
           return layermanager->getViewport();

       // Return an empty QRectF if there is no layermanager
       return QRectF();
   }

   bool MapControl::isGeometryVisible( Geometry * geometry)
   {
       if ( !geometry || getViewport() == QRectF() )
           return false;

       return getViewport().contains( geometry->boundingBox() );
   }

   int MapControl::loadingQueueSize()
   {
       return ImageManager::instance()->loadQueueSize();
   }

   void MapControl::suspendUpdate(bool suspend)
   {
       if (suspend)
       {
            ++updateSuspendedCounter;
       }
       else
       {
           --updateSuspendedCounter;
           if (updateSuspendedCounter < 0)
               updateSuspendedCounter = 0;
       }
   }

   bool MapControl::isUpdateSuspended()
   {
       return (updateSuspendedCounter > 0) ? true : false;
   }
}
