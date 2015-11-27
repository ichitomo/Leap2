#pragma once

#include "cinder/gl/gl.h"

#include "Leap.h"

using namespace ci;

class Paint
{
public:

  Paint()
    : mIs3D( false )
  {
  }

  void clear()
  {
    // ‹OÕ‚ðƒNƒŠƒA‚·‚é
    mCompleteStrokes.clear();
    mCurrentStrokes.clear();
  }

  void update()
  {
    mCurrentFrame = mLeap.frame();

    mPointablePoints.clear();

    auto pointables = mCurrentFrame.pointables();
    for ( auto pointable : pointables ) {
      auto point = pointable.tipPosition();
      mPointablePoints.push_back( point );

      if ( isValid( pointable ) ) {
        mCurrentStrokes[pointable.id()].push_back( point );
      }
    }

    for ( auto it = mCurrentStrokes.begin(); it != mCurrentStrokes.end();  ) {
      auto pointable = mCurrentFrame.pointable( it->first );
      if ( isValid( pointable ) ) {
        ++it;
      }
      else {
        mCompleteStrokes.push_back( it->second );
        it = mCurrentStrokes.erase( it );
      }
    }
  }

  void draw()
  {
    gl::clear( Color( 0, 0, 0 ) ); 

    drawPoints( mPointablePoints, 15, Color( 1, 0, 0 ) );

    for ( auto strokes : mCompleteStrokes ) {
      drawLineStrip( strokes );
    }

    for ( auto strokes : mCurrentStrokes ) {
      drawLineStrip( strokes.second );
    }
  }

  void set3DMode( bool is3d )
  {
    mIs3D = is3d;
  }

  bool get3DMode() const
  {
    return mIs3D;
  }

  static const int Width = 1440;
  static const int Height = 900;

private:

  bool isValid( Leap::Pointable pointable )
  {
    return pointable.isValid() &&
           pointable.isExtended();
  }

  void drawLineStrip(const std::vector<Leap::Vector>& strokes)
  {
    gl::lineWidth( 10 );
    setDiffuseColor( Color::white() );
    gl::begin( GL_LINE_STRIP );

    for ( auto position : strokes ) {
      drawVertex( position );
    }

    gl::end();

    drawPoints( strokes, 5, Color::white() );
  }

  void drawPoints( const std::vector<Leap::Vector>& points, int size, Color color )
  {
    glPointSize( size );
    setDiffuseColor( color );
    gl::begin( GL_POINTS );

    for ( auto point : points ) {
      drawVertex( point );
    }
    gl::end();
  }

  void drawVertex( Leap::Vector pointable )
  {
    if ( mIs3D ) {
      gl::vertex( toVec3f( pointable ) );
    }
    else {
      auto box = mCurrentFrame.interactionBox();
      auto point = box.normalizePoint( pointable );

      gl::vertex( point.x * Width, Height  - (point.y * Height) );

      //gl::vertex( point.x * Width, point.z * Height );
    }
  }

  // GL_LIGHT0
  void setDiffuseColor( ci::ColorA diffuseColor )
  {
    gl::color( diffuseColor );
    glLightfv( GL_LIGHT0 , GL_DIFFUSE, diffuseColor );
  }

  // Leap SDK
  Vec3f toVec3f( Leap::Vector vec )
  {
    return Vec3f( vec.x, vec.y, vec.z );
  }

private:

  bool mIs3D;

  // Leap Motion
  Leap::Controller mLeap;
  Leap::Frame mCurrentFrame;

  std::vector<std::vector<Leap::Vector>> mCompleteStrokes;
  std::map<int, std::vector<Leap::Vector>> mCurrentStrokes;
  std::vector<Leap::Vector> mPointablePoints;
};

