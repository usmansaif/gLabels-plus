//  Handles.cpp
//
//  Copyright (C) 2013-2026  Jaye Evins <evins@snaught.com>
//
//  This file is part of gLabels-qt.
//
//  gLabels-qt is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  gLabels-qt is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with gLabels-qt.  If not, see <http://www.gnu.org/licenses/>.
//


#include "Handle.hpp"

#include "ModelObject.hpp"

#include <QColor>
#include <QDebug>


//
// Private
//
namespace
{
        const double handlePixels             = 7;
        const double handleOutlineWidthPixels = 1;
        const double cornerGrabPixels         = 11;   // corners and line ends: easier to grab than they look
        const double edgeGrabPixels           = 4;    // edges: this far either side of the edge line

        const QColor handleFillColor( 0,  192,  0,  96 );
        const QColor originHandleFillColor( 192,  0,  0,  96 );
        const QColor handleOutlineColor( 0,  0,   0,  192 );
}


namespace glabels::model
{

        ///
        /// Handle Constructor
        ///
        Handle::Handle( ModelObject* owner, Location location )
                : mOwner(owner), mLocation(location)
        {
                // empty
        }


        ///
        /// Is Handle null?
        ///
        bool Handle::isNull() const
        {
                return mLocation == NULL_HANDLE;
        }


        ///
        /// Handle owner
        ///
        ModelObject* Handle::owner() const
        {
                return mOwner;
        }


        ///
        /// Handle location
        ///
        Handle::Location Handle::location() const
        {
                return mLocation;
        }


        ///
        /// Draw Handle
        ///
        void Handle::draw( QPainter* painter, double scale ) const
        {
                switch ( mLocation )
                {
                case NW:
                        drawAt( painter, scale, 0, 0, originHandleFillColor );
                        break;
                case N:
                        drawAt( painter, scale, mOwner->w()/2, 0, handleFillColor );
                        break;
                case NE:
                        drawAt( painter, scale, mOwner->w(), 0, handleFillColor );
                        break;
                case E:
                        drawAt( painter, scale, mOwner->w(), mOwner->h()/2, handleFillColor );
                        break;
                case SE:
                        drawAt( painter, scale, mOwner->w(), mOwner->h(), handleFillColor );
                        break;
                case S:
                        drawAt( painter, scale, mOwner->w()/2, mOwner->h(), handleFillColor );
                        break;
                case SW:
                        drawAt( painter, scale, 0, mOwner->h(), handleFillColor );
                        break;
                case W:
                        drawAt( painter, scale, 0, mOwner->h()/2, handleFillColor );
                        break;
                case P1:
                        drawAt( painter, scale, 0, 0, originHandleFillColor );
                        break;
                case P2:
                        drawAt( painter, scale, mOwner->w(), mOwner->h(), handleFillColor );
                        break;
                default:
                        qWarning() << "Unknown Handle location";
                        break;
                }
        }


        ///
        /// Handle Path
        ///
        QPainterPath Handle::path( double scale ) const
        {
                switch ( mLocation )
                {
                case NW:
                        return pathAt( scale, 0, 0 );
                        break;
                case N:
                        return pathAt( scale, mOwner->w()/2, 0 );
                        break;
                case NE:
                        return pathAt( scale, mOwner->w(), 0 );
                        break;
                case E:
                        return pathAt( scale, mOwner->w(), mOwner->h()/2 );
                        break;
                case SE:
                        return pathAt( scale, mOwner->w(), mOwner->h() );
                        break;
                case S:
                        return pathAt( scale, mOwner->w()/2, mOwner->h() );
                        break;
                case SW:
                        return pathAt( scale, 0, mOwner->h() );
                        break;
                case W:
                        return pathAt( scale, 0, mOwner->h()/2 );
                        break;
                case P1:
                        return pathAt( scale, 0, 0 );
                        break;
                case P2:
                        return pathAt( scale, mOwner->w(), mOwner->h() );
                        break;
                default:
                        qWarning() << "Unknown Handle location";
                        return QPainterPath(); // Empty
                        break;
                }
        }


        bool Handle::isEdge() const
        {
                return mLocation == N || mLocation == E || mLocation == S || mLocation == W;
        }


        QPointF Handle::direction() const
        {
                switch ( mLocation )
                {
                case NW: return QPointF( -1, -1 );
                case N:  return QPointF(  0, -1 );
                case NE: return QPointF(  1, -1 );
                case E:  return QPointF(  1,  0 );
                case SE: return QPointF(  1,  1 );
                case S:  return QPointF(  0,  1 );
                case SW: return QPointF( -1,  1 );
                case W:  return QPointF( -1,  0 );
                case P1: return QPointF( -mOwner->w().pt(), -mOwner->h().pt() );
                case P2: return QPointF(  mOwner->w().pt(),  mOwner->h().pt() );
                default: return QPointF();
                }
        }


        QPainterPath Handle::hitPath( double scale ) const
        {
                const double s = 1/scale;
                const double w = mOwner->w().pt();
                const double h = mOwner->h().pt();
                const double c = s*cornerGrabPixels/2;   // half the corner grab square
                const double e = s*edgeGrabPixels;

                QPainterPath path;
                switch ( mLocation )
                {
                case N:
                        path.addRect( QRectF( QPointF( qMin( c, w/2 ), -e ), QPointF( qMax( w - c, w/2 ), e ) ).normalized() );
                        break;
                case S:
                        path.addRect( QRectF( QPointF( qMin( c, w/2 ), h - e ), QPointF( qMax( w - c, w/2 ), h + e ) ).normalized() );
                        break;
                case W:
                        path.addRect( QRectF( QPointF( -e, qMin( c, h/2 ) ), QPointF( e, qMax( h - c, h/2 ) ) ).normalized() );
                        break;
                case E:
                        path.addRect( QRectF( QPointF( w - e, qMin( c, h/2 ) ), QPointF( w + e, qMax( h - c, h/2 ) ) ).normalized() );
                        break;
                default:
                {
                        // Corners and line ends: a larger square round the drawn handle
                        QPainterPath square;
                        square.addRect( -c, -c, 2*c, 2*c );
                        QPointF at = this->path( scale ).boundingRect().center();
                        square.translate( at );
                        path = square;
                        break;
                }
                }
                return path;
        }


        ///
        /// Draw Handle at x,y
        ///
        void Handle::drawAt( QPainter* painter,
                             double    scale,
                             Distance  x,
                             Distance  y,
                             QColor    color ) const
        {
                painter->save();

                painter->translate( x.pt(), y.pt() );

                double s = 1.0 / scale;

                QPen pen( handleOutlineColor );
                pen.setCosmetic( true );
                pen.setWidth( handleOutlineWidthPixels );

                painter->setPen( pen );
                painter->setBrush( color );

                painter->drawRect( QRectF( -s*handlePixels/2.0, -s*handlePixels/2.0,
                                           s*handlePixels, s*handlePixels ) );

                painter->restore();
        }


        ///
        /// Create Handle path at x,y
        ///
        QPainterPath Handle::pathAt( double   scale,
                                     Distance x,
                                     Distance y ) const
        {
                QPainterPath path;

                double s = 1/scale;

                path.addRect( -s*handlePixels/2, -s*handlePixels/2, s*handlePixels, s*handlePixels );
                path.translate( x.pt(), y.pt() );

                return path;
        }


}
