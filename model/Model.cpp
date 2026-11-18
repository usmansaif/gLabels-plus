//  Model.cpp
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


#include "Model.hpp"

#include "ModelObject.hpp"
#include "ModelTextObject.hpp"
#include "ModelImageObject.hpp"
#include "Region.hpp"
#include "Size.hpp"
#include "XmlLabelCreator.hpp"
#include "XmlLabelParser.hpp"

#include "merge/None.hpp"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFileInfo>


//
// Private
//
namespace
{
        const QString MIME_TYPE = "application/x-glabels-objects";
}


namespace glabels::model
{
        ///
        /// Default constructor.
        ///
        Model::Model()
        {
                mMerge.reset( new merge::None() );

                connect( &mVariables, SIGNAL(changed()), this, SLOT(onVariablesChanged()) );
        }


        ///
        /// Destructor.
        ///
        Model::~Model()
        {
                qDeleteAll( mObjectList );
        }


        ///
        /// Save model state
        ///
        Model* Model::save() const
        {
                auto* savedModel = new Model(); // mMerge shared between models

                if ( mFileName.isEmpty() && mUntitledInstance == 0 )
                {
                        qDebug() << "Model::save: Warning: called before mUntitledInstance has been initialized: untitled names will differ";
                }

                savedModel->restore( this );

                return savedModel;
        }


        ///
        /// Restore model state
        ///
        void Model::restore( const Model *savedModel )
        {
                // Clear current object list
                mKeyObject = nullptr;
                qDeleteAll( mObjectList );
                mObjectList.clear();

                // Now copy state
                mUntitledInstance = savedModel->mUntitledInstance;
                mModified         = savedModel->mModified;
                mFileName         = savedModel->mFileName;
                mTmplate          = savedModel->mTmplate;
                mRotate           = savedModel->mRotate;

                for ( ModelObject* savedObject : savedModel->mObjectList )
                {
                        ModelObject* object = savedObject->clone();

                        object->setParent( this );
                        mObjectList << object;

                        connect( object, SIGNAL(changed()), this, SLOT(onObjectChanged()) );
                        connect( object, SIGNAL(moved()), this, SLOT(onObjectMoved()) );
                }

                mVariables.copy( savedModel->mVariables );

                mMerge = savedModel->mMerge;

                // Emit signals based on potential changes
                emit changed();
                emit selectionChanged();
                emit modifiedChanged();
                emit nameChanged();
                emit sizeChanged();
        }


        ///
        /// Is model modified?
        ///
        bool Model::isModified() const
        {
                return mModified;
        }


        ///
        /// Get filename
        ///
        const QString& Model::fileName() const
        {
                return mFileName;
        }


        ///
        /// Set filename
        ///
        void Model::setFileName( const QString &fileName )
        {
                if ( mFileName != fileName )
                {
                        mFileName = fileName;
                        emit nameChanged();
                }
        }


        ///
        /// Get template
        ///
        const Template& Model::tmplate() const
        {
                return mTmplate;
        }


        ///
        /// Get frame
        ///
        const Frame* Model::frame( const QString& id ) const
        {
                return mTmplate.frame( id );
        }


        ///
        /// Set template
        ///
        void Model::setTmplate( const Template& tmplate )
        {
                mTmplate = tmplate;

                setModified();

                emit changed();
                emit sizeChanged();

                Settings::addToRecentTemplateList( tmplate.name() );
        }


        ///
        /// Get rotation
        ///
        bool Model::rotate() const
        {
                return mRotate;
        }


        ///
        /// Set rotation
        ///
        void Model::setRotate( bool rotate )
        {
                if (mRotate != rotate)
                {
                        mRotate = rotate;

                        setModified();

                        emit changed();
                        emit sizeChanged();
                }
        }


        ///
        /// Get width
        ///
        Distance Model::w() const
        {
                auto frame = mTmplate.frame();
                if ( frame )
                {
                        return mRotate ? frame->h() : frame->w();
                }
                else
                {
                        return Distance::pt(0);
                }
        }


        ///
        /// Get height
        ///
        Distance Model::h() const
        {
                auto frame = mTmplate.frame();
                if ( frame )
                {
                        return mRotate ? frame->w() : frame->h();
                }
                else
                {
                        return Distance::pt(0);
                }
        }


        ///
        /// Set height (if variable length)
        ///
        void Model::setH( Distance h )
        {
                if ( mTmplate.setH( h ) )
                {
                        setModified();

                        emit changed();
                        emit sizeChanged();
                }
        }


        ///
        /// Get object list
        ///
        const QList<ModelObject*>& Model::objectList() const
        {
                return mObjectList;
        }


        ///
        /// Get directory as a QDir.
        ///
        QDir Model::dir() const
        {
                if ( mFileName.isEmpty() )
                {
                        return QDir::current();
                }
                else
                {
                        return QFileInfo( mFileName ).absoluteDir();
                }
        }


        ///
        /// Get directory as a path.
        ///
        QString Model::dirPath() const
        {
                if ( mFileName.isEmpty() )
                {
                        return QDir::currentPath();
                }
                else
                {
                        return QFileInfo( mFileName ).absolutePath();
                }
        }


        ///
        /// Get short name.
        ///
        QString Model::shortName()
        {
                static int untitledCount = 0;

                if ( mFileName.isEmpty() )
                {
                        if ( mUntitledInstance == 0 )
                        {
                                mUntitledInstance = ++untitledCount;
                        }
                        QString numString;
                        numString.setNum(mUntitledInstance);;

                        return tr("Untitled") + numString;
                }
                else
                {
                        QFileInfo fileInfo( mFileName );
                        return fileInfo.completeBaseName();
                }
        }


        ///
        /// Get variables object
        ///
        Variables& Model::variables()
        {
                return mVariables;
        }


        ///
        /// Get const reference to variables object
        ///
        const Variables& Model::constVariables() const
        {
                return mVariables;
        }


        ///
        /// Get merge object
        ///
        merge::Merge* Model::merge() const
        {
                return mMerge.get();
        }


        ///
        /// Set merge object
        ///
        void Model::setMerge( merge::Merge* merge )
        {
                if ( merge != mMerge )
                {
                        mMerge.reset( merge );

                        connect( mMerge.get(), SIGNAL(sourceChanged()), this, SLOT(onMergeSourceChanged()) );
                        connect( mMerge.get(), SIGNAL(selectionChanged()), this, SLOT(onMergeSelectionChanged()) );

                        setModified();

                        emit changed();
                        emit mergeChanged();
                        emit mergeSourceChanged();
                }
        }


        ///
        /// Set modified status
        ///
        void Model::setModified()
        {
                mModified = true;
                emit modifiedChanged();
        }


        ///
        /// Clear modified status
        ///
        void Model::clearModified()
        {
                mModified = false;
                emit modifiedChanged();
        }


        ///
        /// Add object.
        ///
        void Model::addObject( ModelObject* object )
        {
                object->setParent( this );
                mObjectList << object;

                connect( object, SIGNAL(changed()), this, SLOT(onObjectChanged()) );
                connect( object, SIGNAL(moved()), this, SLOT(onObjectMoved()) );

                setModified();

                emit changed();
        }


        ///
        /// Delete Object
        ///
        void Model::deleteObject( ModelObject* object )
        {
                object->unselect();
                mObjectList.removeOne( object );

                disconnect( object, nullptr, this, nullptr );

                setModified();

                emit changed();

                delete object;
        }


        ///
        /// Object at x,y
        ///
        ModelObject* Model::objectAt( double   scale,
                                      Distance x,
                                      Distance y ) const
        {
                /* Search object list in reverse order.  I.e. from top to bottom. */
                QList<ModelObject*>::const_iterator it = mObjectList.end();
                while ( it != mObjectList.begin() )
                {
                        it--;
                        ModelObject* object = *it;
                        if ( !object->isLocked() && object->isLocatedAt( scale, x, y ) )
                        {
                                return object;
                        }
                }

                return nullptr;
        }


        ///
        /// Handle at x,y
        ///
        const Handle& Model::handleAt( double   scale,
                                       Distance x,
                                       Distance y ) const
        {
                static Handle nullHandle;

                for( ModelObject* object : mObjectList )
                {
                        auto& handle = object->handleAt( scale, x, y );
                        if ( !handle.isNull() )
                        {
                                return handle;
                        }
                }

                return nullHandle;
        }


        ///
        /// Object Changed Slot
        ///
        void Model::onObjectChanged()
        {
                setModified();
                emit changed();
        }


        ///
        /// Object Moved Slot
        ///
        void Model::onObjectMoved()
        {
                setModified();
                emit changed();
        }


        ///
        /// Variables Changed Slot
        ///
        void Model::onVariablesChanged()
        {
                setModified();
                emit changed();
                emit variablesChanged();
        }


        ///
        /// Merge Source Changed Slot
        ///
        void Model::onMergeSourceChanged()
        {
                setModified();
                emit changed();
                emit mergeSourceChanged();
        }


        ///
        /// Merge Selection Changed Slot
        ///
        void Model::onMergeSelectionChanged()
        {
                emit changed();
                emit mergeSelectionChanged();
        }


        ///
        /// Select Object
        ///
        void Model::selectObject( ModelObject* object )
        {
                if ( object->isLocked() )
                {
                        return;
                }

                object->select();
                mKeyObject = object;

                emit selectionChanged();
        }


        ///
        /// Unselect Object
        ///
        void Model::unselectObject( ModelObject* object )
        {
                object->unselect();
                if ( mKeyObject == object )
                {
                        mKeyObject = nullptr;
                }

                emit selectionChanged();
        }


        ///
        /// Select All Objects
        ///
        void Model::selectAll()
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( !object->isLocked() )
                        {
                                object->select();
                        }
                }

                emit selectionChanged();
        }


        ///
        /// Unselect All Objects
        ///
        void Model::unselectAll()
        {
                for ( ModelObject* object : mObjectList )
                {
                        object->unselect();
                }
                mKeyObject = nullptr;

                emit selectionChanged();
        }


        ///
        /// Select Region
        ///
        void Model::selectRegion( const Region &region )
        {
                Distance rX1 = min( region.x1(), region.x2() );
                Distance rY1 = min( region.y1(), region.y2() );
                Distance rX2 = max( region.x1(), region.x2() );
                Distance rY2 = max( region.y1(), region.y2() );

                for ( ModelObject* object : mObjectList )
                {
                        Region objectExtent = object->getExtent();

                        if ( !object->isLocked() &&
                             (objectExtent.x1() >= rX1) &&
                             (objectExtent.x2() <= rX2) &&
                             (objectExtent.y1() >= rY1) &&
                             (objectExtent.y2() <= rY2) )
                        {
                                object->select();
                        }
                }

                emit selectionChanged();
        }


        ///
        /// Is Selection Empty?
        ///
        bool Model::isSelectionEmpty()
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                return false;
                        }
                }

                return true;
        }


        ///
        /// Is Selection Atomic?
        ///
        bool Model::isSelectionAtomic()
        {
                int nSelected = 0;

                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                nSelected++;
                                if ( nSelected > 1 )
                                {
                                        return false;
                                }
                        }
                }

                return nSelected == 1;
        }


        ///
        /// Get List of Selected Objects
        ///
        QList<ModelObject*> Model::getSelection()
        {
                QList<ModelObject*> selectedList;

                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                selectedList << object;
                        }
                }

                return selectedList;
        }


        ///
        /// Get First Object in Selection List
        ///
        ModelObject* Model::getFirstSelectedObject()
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                return object;
                        }
                }

                return nullptr;
        }


        ///
        /// Can Any Objects in Selection Accept Text Properties?
        ///
        bool Model::canSelectionText()
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() && object->canText() )
                        {
                                return true;
                        }
                }

                return false;
        }


        ///
        /// Can Any Objects in Selection Accept Fill Property?
        ///
        bool Model::canSelectionFill()
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() && object->canFill() )
                        {
                                return true;
                        }
                }

                return false;
        }


        ///
        /// Can Any Objects in Selection Accept Line Color Property?
        ///
        bool Model::canSelectionLineColor()
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() && object->canLineColor() )
                        {
                                return true;
                        }
                }

                return false;
        }


        ///
        /// Can Any Objects in Selection Accept Line Width Property?
        ///
        bool Model::canSelectionLineWidth()
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() && object->canLineWidth() )
                        {
                                return true;
                        }
                }

                return false;
        }


        ///
        /// Delete Selected Objects
        ///
        void Model::deleteSelection()
        {
                QList<ModelObject*> selectedList = getSelection();

                for ( ModelObject* object : selectedList )
                {
                        deleteObject( object );
                }

                setModified();

                emit changed();
                emit selectionChanged();
        }


        ///
        /// Raise Selected Objects To Top
        ///
        void Model::raiseSelectionToTop()
        {
                QList<ModelObject*> selectedList = getSelection();

                for ( ModelObject* object : selectedList )
                {
                        mObjectList.removeOne( object );
                }

                // Move to end of list, representing top most object.
                for ( ModelObject* object : selectedList )
                {
                        mObjectList.push_back( object );
                }

                setModified();

                emit changed();
        }


        ///
        /// Lower Selected Objects To Bottom
        ///
        void Model::lowerSelectionToBottom()
        {
                QList<ModelObject*> selectedList = getSelection();

                for ( ModelObject* object : selectedList )
                {
                        mObjectList.removeOne( object );
                }

                // Move to front of list, representing bottom most object.
                for ( ModelObject* object : selectedList )
                {
                        mObjectList.push_front( object );
                }

                setModified();

                emit changed();
        }


        ///
        /// Raise Selected Objects One Step
        ///
        void Model::raiseSelection()
        {
                // Walk from top to bottom, swapping each selected object with the
                // unselected object directly above it.
                bool changedFlag = false;
                for ( int i = mObjectList.size() - 2; i >= 0; i-- )
                {
                        if ( mObjectList[i]->isSelected() && !mObjectList[i+1]->isSelected() )
                        {
                                mObjectList.swapItemsAt( i, i+1 );
                                changedFlag = true;
                        }
                }

                if ( changedFlag )
                {
                        setModified();
                        emit changed();
                }
        }


        ///
        /// Lower Selected Objects One Step
        ///
        void Model::lowerSelection()
        {
                bool changedFlag = false;
                for ( int i = 1; i < mObjectList.size(); i++ )
                {
                        if ( mObjectList[i]->isSelected() && !mObjectList[i-1]->isSelected() )
                        {
                                mObjectList.swapItemsAt( i, i-1 );
                                changedFlag = true;
                        }
                }

                if ( changedFlag )
                {
                        setModified();
                        emit changed();
                }
        }


        ///
        /// Move object to a new z-order index (0 = bottom)
        ///
        void Model::moveObjectToIndex( ModelObject* object, int index )
        {
                int from = mObjectList.indexOf( object );
                if ( from < 0 )
                {
                        return;
                }
                index = qBound( 0, index, int(mObjectList.size()) - 1 );
                if ( from != index )
                {
                        mObjectList.move( from, index );
                        setModified();
                        emit changed();
                }
        }


        ///
        /// Lock/unlock object
        ///
        void Model::setObjectLocked( ModelObject* object, bool locked )
        {
                object->setLocked( locked );
                emit selectionChanged();
        }


        ///
        /// Set absolute rotation of selected objects
        ///
        void Model::setSelectionRotation( double thetaDegs )
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->setRotation( thetaDegs );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Rotate Selected Objects
        ///
        void Model::rotateSelection( double thetaDegs )
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->rotate( thetaDegs );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Rotate Selected Objects Left 90 degrees
        ///
        void Model::rotateSelectionLeft()
        {
                rotateSelection( -90.0 );
        }


        ///
        /// Rotate Selected Objects Right 90 degrees
        ///
        void Model::rotateSelectionRight()
        {
                rotateSelection( 90.0 );
        }


        ///
        /// Flip Selected Objects Horizontally
        ///
        void Model::flipSelectionHoriz()
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->flipHoriz();
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Flip Selected Objects Vertically
        ///
        void Model::flipSelectionVert()
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->flipVert();
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Set Alignment Reference
        ///
        void Model::setAlignReference( AlignReference ref )
        {
                mAlignReference = ref;
        }


        ///
        /// Get Alignment Reference
        ///
        Model::AlignReference Model::alignReference() const
        {
                return mAlignReference;
        }


        ///
        /// Key Object (most recently selected object)
        ///
        ModelObject* Model::keyObject() const
        {
                if ( mKeyObject && mKeyObject->isSelected() )
                {
                        return mKeyObject.data();
                }
                return nullptr;
        }


        ///
        /// Can selection be aligned using current alignment reference?
        ///
        bool Model::canAlignSelection()
        {
                if ( isSelectionEmpty() )
                {
                        return false;
                }
                if ( mAlignReference == ALIGN_TO_LABEL )
                {
                        return true;
                }
                return !isSelectionAtomic();
        }


        ///
        /// Generic alignment of selection
        ///
        void Model::alignSelection( AlignEdge edge )
        {
                if ( !canAlignSelection() )
                {
                        return;
                }

                QList<ModelObject*> selectedList = getSelection();

                bool horizontal = (edge == ALIGN_LEFT) || (edge == ALIGN_RIGHT) || (edge == ALIGN_HCENTER);

                auto lo  = [&]( const Region& r ) { return horizontal ? r.x1() : r.y1(); };
                auto hi  = [&]( const Region& r ) { return horizontal ? r.x2() : r.y2(); };
                auto mid = [&]( const Region& r ) { return (lo(r) + hi(r)) / 2.0; };

                auto value = [&]( const Region& r ) -> Distance
                {
                        switch ( edge )
                        {
                        case ALIGN_LEFT:
                        case ALIGN_TOP:
                                return lo(r);
                        case ALIGN_RIGHT:
                        case ALIGN_BOTTOM:
                                return hi(r);
                        default:
                                return mid(r);
                        }
                };

                // Determine target coordinate
                Distance target = 0;
                ModelObject* key = keyObject();
                AlignReference ref = mAlignReference;
                if ( ref == ALIGN_TO_KEY_OBJECT && key == nullptr )
                {
                        ref = ALIGN_TO_SELECTION;
                }

                switch ( ref )
                {
                case ALIGN_TO_LABEL:
                {
                        Distance extent = horizontal ? w() : h();
                        switch ( edge )
                        {
                        case ALIGN_LEFT:
                        case ALIGN_TOP:
                                target = 0;
                                break;
                        case ALIGN_RIGHT:
                        case ALIGN_BOTTOM:
                                target = extent;
                                break;
                        default:
                                target = extent / 2.0;
                                break;
                        }
                }
                break;

                case ALIGN_TO_KEY_OBJECT:
                        target = value( key->getExtent() );
                        break;

                default:
                {
                        // ALIGN_TO_SELECTION
                        if ( edge == ALIGN_LEFT || edge == ALIGN_TOP )
                        {
                                target = 7200;
                                for ( auto* object : selectedList )
                                {
                                        target = min( target, lo( object->getExtent() ) );
                                }
                        }
                        else if ( edge == ALIGN_RIGHT || edge == ALIGN_BOTTOM )
                        {
                                target = -7200;
                                for ( auto* object : selectedList )
                                {
                                        target = max( target, hi( object->getExtent() ) );
                                }
                        }
                        else
                        {
                                // Use center of object closest to average center
                                Distance sum = 0;
                                for ( auto* object : selectedList )
                                {
                                        sum += mid( object->getExtent() );
                                }
                                Distance avg = sum / selectedList.size();

                                Distance dmin = 7200;
                                target = avg;
                                for ( auto* object : selectedList )
                                {
                                        Distance c = mid( object->getExtent() );
                                        Distance d = fabs( avg - c );
                                        if ( d < dmin )
                                        {
                                                dmin = d;
                                                target = c;
                                        }
                                }
                        }
                }
                break;
                }

                // Move objects
                for ( auto* object : selectedList )
                {
                        Distance delta = target - value( object->getExtent() );
                        if ( horizontal )
                        {
                                object->setPositionRelative( delta, 0 );
                        }
                        else
                        {
                                object->setPositionRelative( 0, delta );
                        }
                }

                setModified();

                emit changed();
        }


        void Model::alignSelectionLeft()    { alignSelection( ALIGN_LEFT ); }
        void Model::alignSelectionRight()   { alignSelection( ALIGN_RIGHT ); }
        void Model::alignSelectionHCenter() { alignSelection( ALIGN_HCENTER ); }
        void Model::alignSelectionTop()     { alignSelection( ALIGN_TOP ); }
        void Model::alignSelectionBottom()  { alignSelection( ALIGN_BOTTOM ); }
        void Model::alignSelectionVCenter() { alignSelection( ALIGN_VCENTER ); }


        ///
        /// Align Selected Objects To Center Of Label Horizontally
        ///
        void Model::centerSelectionHoriz()
        {
                Distance xLabelCenter = w() / 2.0;

                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                Region r = object->getExtent();
                                Distance xObjectCenter = (r.x1() + r.x2()) / 2.0;
                                Distance dx = xLabelCenter - xObjectCenter;
                                object->setPositionRelative( dx, 0 );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Align Selected Objects To Center Of Label Both Horizontally and Vertically
        ///
        void Model::centerSelection()
        {
                Distance xLabelCenter = w() / 2.0;
                Distance yLabelCenter = h() / 2.0;

                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                Region r = object->getExtent();
                                Distance xObjectCenter = (r.x1() + r.x2()) / 2.0;
                                Distance yObjectCenter = (r.y1() + r.y2()) / 2.0;
                                Distance dx = xLabelCenter - xObjectCenter;
                                Distance dy = yLabelCenter - yObjectCenter;
                                object->setPositionRelative( dx, dy );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Align Selected Objects To Center Of Label Vertically
        ///
        void Model::centerSelectionVert()
        {
                Distance yLabelCenter = h() / 2.0;

                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                Region r = object->getExtent();
                                Distance yObjectCenter = (r.y1() + r.y2()) / 2.0;
                                Distance dy = yLabelCenter - yObjectCenter;
                                object->setPositionRelative( 0, dy );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Move Selected Objects By dx,dy
        ///
        void Model::moveSelection( Distance dx, Distance dy )
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->setPositionRelative( dx, dy );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Set Font Family Of Selected Objects
        ///
        void Model::setSelectionFontFamily( const QString &fontFamily )
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->setFontFamily( fontFamily );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Set Font Size Of Selected Objects
        ///
        void Model::setSelectionFontSize( double fontSize )
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->setFontSize( fontSize );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Set Font Weight Of Selected Objects
        ///
        void Model::setSelectionFontWeight( QFont::Weight fontWeight )
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->setFontWeight( fontWeight );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Set Font Italic Flag Of Selected Objects
        ///
        void Model::setSelectionFontItalicFlag( bool fontItalicFlag )
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->setFontItalicFlag( fontItalicFlag );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Set Text Horizontal Alignment Of Selected Objects
        ///
        void Model::setSelectionTextHAlign( Qt::Alignment textHAlign )
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->setTextHAlign( textHAlign );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Set Text Vertical Alignment Of Selected Objects
        ///
        void Model::setSelectionTextVAlign( Qt::Alignment textVAlign )
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->setTextVAlign( textVAlign );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Set Text Line Spacing Of Selected Objects
        ///
        void Model::setSelectionTextLineSpacing( double textLineSpacing )
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->setTextLineSpacing( textLineSpacing );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Set Text Color Node Of Selected Objects
        ///
        void Model::setSelectionTextColorNode( ColorNode textColorNode )
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->setTextColorNode( textColorNode );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Set Line Width Of Selected Objects
        ///
        void Model::setSelectionLineWidth( Distance lineWidth )
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->setLineWidth( lineWidth );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Set Line Color Node Of Selected Objects
        ///
        void Model::setSelectionLineColorNode( ColorNode lineColorNode )
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->setLineColorNode( lineColorNode );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Set Fill Color Node Of Selected Objects
        ///
        void Model::setSelectionFillColorNode( ColorNode fillColorNode )
        {
                for ( ModelObject* object : mObjectList )
                {
                        if ( object->isSelected() )
                        {
                                object->setFillColorNode( fillColorNode );
                        }
                }

                setModified();

                emit changed();
        }


        ///
        /// Copy selection to clipboard
        ///
        void Model::copySelection()
        {
                if ( !isSelectionEmpty() )
                {
                        QClipboard *clipboard = QApplication::clipboard();

                        auto buffer = XmlLabelCreator::serializeObjects( getSelection(), this );

                        auto *mimeData = new QMimeData;
                        mimeData->setData( MIME_TYPE, buffer );

                        clipboard->setMimeData( mimeData );
                }
        }


        ///
        /// Cut selection to clipboard
        ///
        void Model::cutSelection()
        {
                copySelection();
                deleteSelection();
        }


        ///
        /// Can we paste?
        ///
        bool Model::canPaste()
        {
                const QClipboard *clipboard = QApplication::clipboard();
                const QMimeData *mimeData = clipboard ? clipboard->mimeData() : nullptr;
                if ( !mimeData )
                {
                        return false;
                }

                return  mimeData->hasFormat( MIME_TYPE ) ||
                        mimeData->hasUrls()              ||
                        mimeData->hasImage()             ||
                        mimeData->hasText();
        }


        ///
        /// Paste from clipboard
        ///
        void Model::paste( Point p )
        {
                const QClipboard *clipboard = QApplication::clipboard();
                const QMimeData *mimeData = clipboard->mimeData();

                if ( mimeData->hasFormat( MIME_TYPE ) )
                {
                        pasteAsNativeObjects( mimeData, p );
                }
                else if ( mimeData->hasUrls() )
                {
                        pasteAsUrls( mimeData, p );
                }
                else if ( mimeData->hasImage() )
                {
                        pasteAsImage( mimeData, p );
                }
                else if ( mimeData->hasText() )
                {
                        pasteAsText( mimeData, p );
                }
        }


        ///
        /// Paste as native objects
        ///
        void Model::pasteAsNativeObjects( const QMimeData* mimeData, Point p )
        {
                QByteArray buffer = mimeData->data( MIME_TYPE );
                QList <ModelObject*> objects = XmlLabelParser::deserializeObjects( buffer, this );

                unselectAll();
                for ( ModelObject* object : objects )
                {
                        object->setPositionRelative( p.x(), p.y() );
                        addObject( object );
                        selectObject( object );
                }
        }


        ///
        /// Paste as URLs ( currently only supports local image files )
        ///
        void Model::pasteAsUrls( const QMimeData* mimeData, Point p )
        {
                auto x = p.x();
                auto y = p.y();
                auto xOffset = Distance::pt( 10 );
                auto yOffset = Distance::pt( 10 );

                unselectAll();
                for ( auto url : mimeData->urls() )
                {
                        if ( url.isLocalFile() )
                        {
                                auto name = url.toLocalFile();
                                QImage image( name );
                                if ( !image.isNull() )
                                {
                                        auto* object = new ModelImageObject();
                                        object->setImage( name, image );
                                        object->setSize( object->naturalSize() );
                                        object->setPosition( x, y );
                                        addObject( object );
                                        selectObject( object );

                                        x = fmod( x + xOffset, w() );
                                        y = fmod( y + yOffset, h() );
                                }
                                else
                                {
                                        qWarning() << "Cannot paste" << name
                                                   << ": does not exist or currently unsupported file type.";
                                }
                        }
                        else
                        {
                                qWarning() << "Cannot paste" << url.toString()
                                           << ": currently unsupported file location.";
                        }
                }
        }


        ///
        /// Paste as image
        ///
        void Model::pasteAsImage( const QMimeData* mimeData, Point p )
        {
                auto* object = new ModelImageObject();
                object->setImage( qvariant_cast<QImage>(mimeData->imageData()) );
                object->setSize( object->naturalSize() );
                object->setPosition( p.x(), p.y() );
                addObject( object );
                unselectAll();
                selectObject( object );
        }


        ///
        /// Paste as text
        ///
        void Model::pasteAsText( const QMimeData* mimeData, Point p )
        {
                auto* object = new ModelTextObject();
                object->setText( mimeData->text() );
                object->setSize( object->naturalSize() );
                object->setPosition( p.x(), p.y() );
                addObject( object );
                unselectAll();
                selectObject( object );
        }


        ///
        /// Draw label objects
        ///
        void Model::draw( QPainter*            painter,
                          bool                 inEditor,
                          const merge::Record& record,
                          const Variables&     variablesInstance ) const
        {
                for ( ModelObject* object : mObjectList )
                {
                        object->draw( painter, inEditor, record, variablesInstance );
                }
        }

}
