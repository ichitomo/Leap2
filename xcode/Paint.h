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
    
    void clear(){
        //軌跡を消す
        mCompleteStrokes.clear();
        mCurrentStrokes.clear();
    }
    
    void update(){
        mCurrentFrame = mLeap.frame();
        Leap::Finger finger = mLeap.frame().fingers().fingerType( Leap::Finger::Type::TYPE_INDEX )[0];
        // InteractionBoxの座標に変換する
        Leap::InteractionBox iBox = mLeap.frame().interactionBox();
        Leap::Vector normalizedPosition =
        iBox.normalizePoint( finger.stabilizedTipPosition() );
        
        // ウィンドウの座標に変換する
        float x = normalizedPosition.x * Width;
        float y = -(Height- (normalizedPosition.y * Height))+Width/2;
        
        mPointablePoints.clear();
        //ポインタブルオブジェクトの座標を取得する
        auto pointables = mCurrentFrame.pointables();
        for ( auto pointable : pointables ) {
            auto point = pointable.tipPosition();
            point.x = x;
            point.y = y;
            mPointablePoints.push_back( point );
            //有効なオブジェクトであれば描き途中に追加
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
        //現在のポインタブル・オブジェクトの位置の表示
        drawPoints( mPointablePoints, 15, Color( 1, 0, 0 ) );
        
        for ( auto strokes : mCompleteStrokes ) {
            //完了した軌跡を描く
            drawLineStrip( strokes );
        }
        
        for ( auto strokes : mCurrentStrokes ) {
            //描き途中の軌跡を描く
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
    
    bool isValid( Leap::Pointable pointable ){
        //有効なポインタ（オブジェクトが有効で退いている）
        return pointable.isValid() &&
        pointable.isExtended();
    }
    //線を描く
    void drawLineStrip(const std::vector<Leap::Vector>& strokes){
        gl::lineWidth( 10 );
        setDiffuseColor( Color::white() );
        gl::begin( GL_LINE_STRIP );
        
        for ( auto position : strokes ) {
            drawVertex( position );
        }
        
        gl::end();
        //線と線の保管
        drawPoints( strokes, 5, Color::white() );
    }
    //点を描く
    void drawPoints( const std::vector<Leap::Vector>& points, int size, Color color ){
        glPointSize( size );
        //setDiffuseColor( color );
        gl::begin( GL_POINTS );
        
        for ( auto point : points ) {
            drawVertex( point );
        }
        gl::end();
    }
    
    void drawVertex( Leap::Vector pointable )
    {
        if ( mIs3D ) {
            //３次元データで描く
            gl::vertex( toVec3f( pointable ) );
        }
        else {
            //２次元データで描く
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

