#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"


#include "cinder/gl/Texture.h"//画像を描画させたいときに使う
#include "cinder/Text.h"
#include "cinder/MayaCamUI.h"
#include "Leap.h"
#include "Paint.h"//絵を描くためのもの
#include "LeapMath.h"
#include "cinder/params/Params.h"//パラメーターを動的に扱える
#include "cinder/ImageIo.h"//画像を描画させたいときに使う
#include "cinder/ObjLoader.h"//
#include "cinder/Utilities.h"//
#include <math.h>
#include "cinder/Capture.h"
#include "cinder/params/Params.h"
#include "time.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//音声解析
#include "cinder/gl/TextureFont.h"
#include "cinder/audio/Context.h"
#include "cinder/audio/MonitorNode.h"
#include "../common/AudioDrawUtils.h"


//ソケット通信
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

//スレッド
#include <unistd.h>

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace cinder::gl;
using namespace cinder::audio;

#define PI 3.141592653589793

#define MAXPOINTS 100//記録できる点の限度
GLint point[MAXPOINTS][2];//点の座標の入れ物
void error(const char *msg){
    //エラーメッセージ
    perror(msg);
    exit(0);
}

//サーバー側のソケット生成をする関数
int sockfd, newsockfd, portno;
socklen_t clilen;
char buffer[256];

char handNumBuff[256];//readしてきたときの手の数
char tapNumBuff[256];//readしてきたときのタップ回数の値
char cirNumBuff[256];//readしてきたときのサークル回数の値

struct sockaddr_in serv_addr, cli_addr;
int l,m,n;//readgの判定に使う
int handCount=0;//手の数をカウントしたときのカウント
int tapCount=0;//ジェスチャーしたときのカウント
int cirCount=0;
int commaCount = 0;

char *hans, *jes1, *jes2, *message;

//ソケットで読み取った値の計算に使う
int handNum = atoi(handNumBuff);//readしてきたときの手の数
int tapNum = atoi(tapNumBuff);//readしてきたときのタップ回数の値
int cirNum = atoi(cirNumBuff);//readしてきたときのサークル回数の値


//socketとdrawを機能させる
char flag[] = "9999";

void *socketSv_loop(void *p);

class LeapApp : public AppNative {
    
public:
    
    void prepareSettings( Settings *settings ) {
        settings->setFrameRate(60);
    }
    
    void setup(){
        // ウィンドウの位置とサイズを設定
        setWindowPos( 0, 0 );
        setWindowSize( WindowWidth, WindowHeight );
        
        // 光源を追加する
        glEnable( GL_LIGHTING );
        glEnable( GL_LIGHT0 );
        
        // 表示フォントの設定
        mFont = Font( "YuGothic", 20 );
        
        
        // カメラ(視点)の設定
        mCapture = Capture(getWindowWidth(), getWindowHeight());
        mCapture.start();
        
        
        mCameraDistance = 1500.0f;//カメラの距離（z座標）
        mEye			= Vec3f( getWindowWidth()/2, getWindowHeight()/2, mCameraDistance );//位置
        mCenter			= Vec3f( getWindowWidth()/2, getWindowHeight()/2, 0.0f);//カメラのみる先
        //mUp				= Vec3f::yAxis();//頭の方向を表すベクトル
        mCam.setEyePoint( mEye );//カメラの位置
        mCam.setCenterOfInterestPoint(mCenter);//カメラのみる先
        //(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
        //fozyはカメラの画角、値が大きいほど透視が強くなり、絵が小さくなる
        //getWindowAspectRatio()はアスペクト比
        //nNearは奥行きの範囲：手前（全方面）
        //zFarは奥行きの範囲：後方（後方面）
        mCam.setPerspective( 45.0f, getWindowAspectRatio(), 300.0f, 3000.0f );//カメラから見える視界の設定
        
        mMayaCam.setCurrentCam(mCam);
        
        gl::enableAlphaBlending();
        
        // SETUP PARAMS
        mParams = params::InterfaceGl( "LearnHow", Vec2i( 200, 160 ) );
        mParams.addParam( "Scene Rotation", &mSceneRotation, "opened=1" );
        mParams.addSeparator();
        mParams.addParam( "Eye Distance", &mCameraDistance, "min=50.0 max=1500.0 step=50.0 keyIncr=s keyDecr=w" );
        
        
        // アルファブレンディングを有効にする
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        gl::enable(GL_BLEND);
        
        //backgroundImageの読み込み
        backgroundImage = gl::Texture(loadImage(loadResource("../resources/image.jpg")));
        
        // 描画時に奥行きの考慮を有効にする
        gl::enableDepthRead();
        gl::enableDepthWrite();
        
        //スレッドを作る
        pthread_t threadSoc;
        
        pthread_create(&threadSoc, NULL, socketSv_loop, NULL);
        
        //pthread_join(threadSoc,NULL);
        //exit(EXIT_SUCCESS);
        
        A = 100.0;    //振幅を設定
        w = 1.0;    //角周波数を設定
        p = 0.0;    //初期位相を設定
        t = 0.0;    //経過時間を初期化
        t2 = 0.0;    //経過時間を初期化
        
        //3Dのお絵かきモード
        mPaint.set3DMode( !mPaint.get3DMode() );
        
        //音声解析を追加
        
        auto ctx = audio::Context::master();
        
        //入力デバイスノードを使用すると、コンテキストに特殊な方法を使用して作成しますので、プラットフォーム固有です
        mInputDeviceNode = ctx->createInputDeviceNode();
        
        //二重FFTサイズを提供することにより、ウィンドウサイズの、我々が与える「ゼロ・パッド」の分析データを
        //得られたスペクトルデータの解像度の増加。

        auto monitorFormat = audio::MonitorSpectralNode::Format().fftSize( WindowWidth*2 ).windowSize( WindowWidth );
        mMonitorSpectralNode = ctx->makeNode( new audio::MonitorSpectralNode( monitorFormat ) );
        
        mInputDeviceNode >> mMonitorSpectralNode;
        
        //InputDeviceNode（およびすべてのInputNodeサブクラス）は、オーディオを処理するため有効にする必要があります
        mInputDeviceNode->enable();
        ctx->enable();
        
        getWindow()->setTitle( mInputDeviceNode->getDevice()->getName() );
    }
    void setupSocketSv();
    void socketSv();
    // マウスのクリック
    void mouseDown( MouseEvent event ){
        mMayaCam.mouseDown( event.getPos() );
//        if( mSpectrumPlot.getBounds().contains( event.getPos() ) )
//            drawPrintBinInfo( event.getX() );
    }
    
    // マウスのドラッグ
    void mouseDrag( MouseEvent event ){
        mMayaCam.mouseDrag( event.getPos(), event.isLeftDown(),
                           event.isMiddleDown(), event.isRightDown() );
    }
    
    // キーダウン
    void keyDown( KeyEvent event ){
        // Cを押したら軌跡をクリアする
        if ( event.getChar() == event.KEY_c ) {
            mPaint.clear();
        }
        switch( event.getCode() )
        {
            case KeyEvent::KEY_ESCAPE:
                quit();
                break;
            default:
                break;
        }
    }
    // 更新処理
    void update(){
        // フレームの更新
        mLastFrame = mCurrentFrame;
        mCurrentFrame = mLeap.frame();
        
        //お絵かきモードのアップデート処理
        mPaint.update();
        //カメラのアップデート処理
        mEye = Vec3f( 0.0f, 0.0f, mCameraDistance );//距離を変える
        mCamPrep.lookAt( mEye, mCenter, mUp);//カメラの位置、m詰めている先の位置、カメラの頭の方向を表すベクトル
        gl::setMatrices( mCamPrep );
        gl::rotate( mSceneRotation );//カメラの回転
        
        if( mCapture.checkNewFrame() ) {
            imgTexture = gl::Texture(mCapture.getSurface() );
            
        }
        
        
        //音声解析のアップデート処理
        mSpectrumPlot.setBounds( Rectf( 40, 40, (float)getWindowWidth() - 40, (float)getWindowHeight() - 40 ) );
        
        //アップデートごとに一度、メインスレッド上でノードから振幅スペクトルをコピーします。
        mMagSpectrum = mMonitorSpectralNode->getMagSpectrum();
        
        graphUpdate();
    }
    
    //描写処理
//    void *draw(void *p){
//        if(!buffer){
//            gl::clear();
//            gl::pushMatrices();// カメラ位置を設定する
//            gl::setMatrices( mMayaCam.getCamera() );
//            drawMarionette();//マリオネット描写
//            drawListArea();//メッセージリストの表示
//            gl::popMatrices();
//            // パラメーター設定UIを描画する
//        }
//        return NULL;
//    }

    
    //描写処理
    void draw(){
        gl::clear();
        gl::enableDepthRead();
        gl::enableDepthWrite();

        gl::pushMatrices();// カメラ位置を設定する
            gl::setMatrices( mMayaCam.getCamera() );
            drawMarionette();//マリオネット描写
            drawListArea();//メッセージリストの表示
            drawCircle();//サークルで表示
            //drawPainting();//指の軌跡を描く
            //drawAudioAnalyze();//音声解析の描写
            //drawSinGraph();//sinグラフを描く
            drawBarGraph();
            //drawBox();
        gl::popMatrices();
        
        // パラメーター設定UIを描画する
        mParams.draw();
        if( imgTexture ) {
            //バックグラウンドイメージを追加
            gl::draw( backgroundImage, getWindowBounds());
        }else{
            //ロードする間にコメント
            gl::drawString("Loading image please wait..",getWindowCenter());
            
        }
    }
    
    //マリオネット
    void drawMarionette(){
        
        //マリオネットを描く関数
        
        //頭
        gl::pushMatrices();
            setDiffuseColor( ci::ColorA( 0.7f, 0.7f, 0.7f, 1.0f ) );
            glTranslatef(defFaceTransX,defFaceTransY,defFaceTransZ );//位置
            glRotatef(-mRotateMatrix3, 1.0f, 0.0f, 0.0f);//回転
            glScalef( mTotalMotionScale, mTotalMotionScale, mTotalMotionScale );//大きさ
            //glTranslatef(10.0,0.0f,0.0f);//移動
            gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 100, 80, 100 ) );//実体
        gl::popMatrices();
        
        //胴体を描く
        gl::pushMatrices();
            glTranslatef(defBodyTransX,defBodyTransY,defBodyTransZ);//移動
            glScalef( mTotalMotionScale, mTotalMotionScale, mTotalMotionScale );//大きさ
            gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 100, 100, 100 ) );//実体
        gl::popMatrices();
        
        //右腕を描く
        gl::pushMatrices();
            setDiffuseColor( ci::ColorA( 0.7f, 0.7f, 0.7f, 1.0f ) );
            glTranslatef(defRightArmTransX,defArmTransY,defArmTransZ);//移動
            glRotatef(mRotateMatrix2, 1.0f, 1.0f, 0.0f);//回転
            //glTranslatef( mTotalMotionTranslation.x/10.0,mTotalMotionTranslation.y/10.0,0.0f);//移動
            glScalef( mTotalMotionScale/2, mTotalMotionScale/4, mTotalMotionScale/2 );//大きさ
        gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 100,  50, 50 ) );//実体
        gl::popMatrices();
        
        //左腕を描く
        gl::pushMatrices();
            setDiffuseColor( ci::ColorA( 0.7f, 0.7f, 0.7f, 1.0f ) );
            glTranslatef(defLeftArmTransX,defArmTransY,defArmTransZ);//移動
            glRotatef(-mRotateMatrix4, -1.0f, 1.0f, 0.0f);//回転
            //glTranslatef(10.0,10.0,0.0f);//移動
            glScalef( mTotalMotionScale/2, mTotalMotionScale/4, mTotalMotionScale/2 );//大きさ
        gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 100, 50, 50 ) );//実体
        gl::popMatrices();
        
        //右足を描く
        gl::pushMatrices();
            setDiffuseColor( ci::ColorA( 0.7f, 0.7f, 0.7f, 1.0f ) );
            glTranslatef(defBodyTransX+25,defBodyTransY-75,defBodyTransZ);//移動
            glRotatef(mRotateMatrix0, 1.0f, 0.0f, 0.0f);//回転
            glScalef( mTotalMotionScale/4, mTotalMotionScale/2, mTotalMotionScale/2 );//大きさ
            gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 100, 100, 100 ) );//実体
        gl::popMatrices();
        
        //左足を描く
        gl::pushMatrices();
            setDiffuseColor( ci::ColorA( 0.7f, 0.7f, 0.7f, 1.0f ) );
            glTranslatef(defBodyTransX-25,defBodyTransY-75,defBodyTransZ);//移動
            glRotatef(mRotateMatrix5, 1.0f, 0.0f, 0.0f);//回転
            glScalef( mTotalMotionScale/4, mTotalMotionScale/2, mTotalMotionScale/2 );//大きさ
            gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 100, 100, 100 ) );//実体
        gl::popMatrices();
        
    }
    //メッセージリスト
    void drawListArea(){
        stringstream mm;
        
//        gl::pushMatrices();
//        auto tbox0 = TextBox().alignment( TextBox::LEFT ).font( mFont ).text ( mm.str() ).color(Color( 1.0f, 1.0f, 1.0f )).backgroundColor( ColorA( 0, 1.0f, 0, 0 ) );
//        auto mTextTexture = gl::Texture( tbox0.render() );
//        gl::draw( mTextTexture );
//  
//        gl::popMatrices();
    }
    
    //サークル（手の数によって大きくなる球体の描写）
    void drawCircle(){
        //sine, cosineを使った曲線的な拡大縮小///////////////////////////
        //この場合-A*sin(w*radians(t) - p)の計算結果は100.0~-100.0なので、
        //100を足すことによって、0~200にしている。
        
        //y = A*sin(w*(t * PI / 180.0) - p) + 100;

        gl::pushMatrices();
        gl::drawSphere(Vec3f( 360, 675, -300 ), cirCount,  cirCount);//指の位置
        gl::popMatrices();
//        t += speed1;    //時間を進める
//        if(t > 360.0) t = 0.0;
        
        //sine, cosineを使わない直線的な拡大縮小(2D)///////////////////////////
//        eSize += speed2;
//        if(eSize > 100 || eSize < 0) speed2 = -speed2;
//        pushMatrices();
//        gl::drawSolidCircle( Vec2f( -100,100 ), eSize, eSize );//指の位置
//        popMatrices();
    }
    //お絵かき（手の軌跡を描写する）
    void drawPainting(){
        
        // 表示座標系の保持
        gl::pushMatrices();
        
        // カメラ位置を設定する
        gl::setMatrices( mMayaCam.getCamera() );
        
        // 描画
        mPaint.draw();
        gl::popMatrices();
        
    }
    
    //sinグラフを描く
    void drawSinGraph(){
        
        glPushMatrix();
        gl::setMatrices( mMayaCam.getCamera() );
        drawGrid();  //基準線
        //サイン波を点で静止画として描画///////////////////////////
        for (t1 = 0.0; t1 < WindowWidth; t1 += speed) {
            y = A*sin(w*(t1 * PI / 180.0) - p);
            drawSolidCircle(Vec2f(t1, y + WindowHeight/2), 1);  //円を描く
        }
        
        //点のアニメーションを描画////////////////////////////////
        y = A*sin(w*(t2 * PI / 180.0) - p);
        drawSolidCircle(Vec2f(t2, y + WindowHeight/2), 10);  //円を描く
        
        t2 += speed;    //時間を進める
        if (t2 > WindowWidth) t2 = 0.0;    //点が右端まで行ったらになったら原点に戻る
        glPopMatrix();
        
    }
    void drawGrid(){
        glPushMatrix();
        gl::setMatrices( mMayaCam.getCamera() );
        //横線
        glBegin(GL_LINES);
        glVertex2d(WindowWidth/2, 0);
        glVertex2d(WindowWidth/2, WindowHeight);
        glEnd();
        //横線
        glBegin(GL_LINES);
        glVertex2d(0, WindowHeight/2);
        glVertex2d(WindowWidth, WindowHeight/2);
        glEnd();
        glPopMatrix();
    }
    //時間ごとに座標を記録する関数
    void graphUpdate(){
        //時間が１秒経つごとに座標を配列に記録していく
        if (time(&next) != last){
            last = next;
            pastSec++;
            printf("%d 秒経過\n", pastSec);
            point[pastSec][0]=pastSec;
            point[pastSec][1]=handNum;
            std::cout << "graphUpdate関数"<<"\n"
            << "秒数：" << pastSec << "\n"
            << "手の数：" << handNum << "\n"
            << std::endl;
            
        }
    }
    //棒グラフを描く
    void drawBarGraph(){
        for (int i = 0; i < pastSec; i++) {
            //棒グラフを描写していく
            glPushMatrix();
                glBegin(GL_LINE_STRIP);
                glColor3d(1.0, 0.0, 0.0);
                glLineWidth(10);
                glVertex2d(point[i][0]*10, 0);//x座標
                glVertex2d(point[i][0]*10 , point[i][1]*100);//y座標
                glEnd();
            glPopMatrix();
            
        }
    }
    //枠としてのBoxを描く
    void drawBox(){
        // 上面
        gl::drawLine( Vec3f( mLeft, mTop, mBackSide ),
                     Vec3f( mRight, mTop, mBackSide ) );
        
        gl::drawLine( Vec3f( mRight, mTop, mBackSide ),
                     Vec3f( mRight, mTop, mFrontSide ) );
        
        gl::drawLine( Vec3f( mRight, mTop, mFrontSide ),
                     Vec3f( mLeft, mTop, mFrontSide ) );
        
        gl::drawLine( Vec3f( mLeft, mTop, mFrontSide ),
                     Vec3f( mLeft, mTop, mBackSide ) );
        
//        //2/3面
//        gl::drawLine( Vec3f( mLeft, mTop*2/3, mBackSide ),
//                     Vec3f( mRight, mTop*2/3, mBackSide ) );
//        
//        gl::drawLine( Vec3f( mRight, mTop*2/3, mBackSide ),
//                     Vec3f( mRight, mTop*2/3, mFrontSide ) );
//        
//        gl::drawLine( Vec3f( mRight, mTop*2/3, mFrontSide ),
//                     Vec3f( mLeft, mTop*2/3, mFrontSide ) );
//        
//        gl::drawLine( Vec3f( mLeft, mTop*2/3, mFrontSide ),
//                     Vec3f( mLeft, mTop*2/3, mBackSide ) );
//        
        // 中面
        gl::drawLine( Vec3f( mLeft, mTop/2, mBackSide ),
                     Vec3f( mRight, mTop/2, mBackSide ) );
        
        gl::drawLine( Vec3f( mRight, mTop/2, mBackSide ),
                     Vec3f( mRight, mTop/2, mFrontSide ) );
        
        gl::drawLine( Vec3f( mRight, mTop/2, mFrontSide ),
                     Vec3f( mLeft, mTop/2, mFrontSide ) );
        
        gl::drawLine( Vec3f( mLeft, mTop/2, mFrontSide ),
                     Vec3f( mLeft, mTop/2, mBackSide ) );
        
        
        //中心線
        gl::drawLine( Vec3f( mLeft, mTop/2, 0 ),
                     Vec3f( mRight, mTop/2, 0 ) );

        gl::drawLine( Vec3f( 0, mTop/2, mBackSide ),
                     Vec3f( 0, mTop/2, mFrontSide ) );
        
        gl::drawLine( Vec3f( mRight/2, 0, 0 ),
                     Vec3f( mRight/2, mTop, 0 ) );
        
        
        // 下面
        gl::drawLine( Vec3f( mLeft, mBottom, mBackSide ),
                     Vec3f( mRight, mBottom, mBackSide ) );
        
        gl::drawLine( Vec3f( mRight, mBottom, mBackSide ),
                     Vec3f( mRight, mBottom, mFrontSide ) );
        
        gl::drawLine( Vec3f( mRight, mBottom, mFrontSide ),
                     Vec3f( mLeft, mBottom, mFrontSide ) );
        
        gl::drawLine( Vec3f( mLeft, mBottom, mFrontSide ),
                     Vec3f( mLeft, mBottom, mBackSide ) );
        
        // 側面
        gl::drawLine( Vec3f( mLeft, mTop, mFrontSide ),
                     Vec3f( mLeft, mBottom, mFrontSide ) );
        
        gl::drawLine( Vec3f( mLeft, mTop, mBackSide ),
                     Vec3f( mLeft, mBottom, mBackSide ) );
        
        gl::drawLine( Vec3f( mRight, mTop, mFrontSide ),
                     Vec3f( mRight, mBottom, mFrontSide ) );
        
        gl::drawLine( Vec3f( mRight, mTop, mBackSide ),
                     Vec3f( mRight, mBottom, mBackSide ) );
    
    }
    
    //音声解析の描写
    /*void drawAudioAnalyze(){
        glPushMatrix();
        mSpectrumPlot.draw( mMagSpectrum );
        //drawLabels();
        glPopMatrix();
    }
    
    //音声解析のラベルの描写
    void drawLabels(){
        if( ! mTextureFont )
            mTextureFont = gl::TextureFont::create( Font( Font::getDefault().getName(), 16 ) );
        gl::color( 0, 0.9f, 0.9f );
        
        // x座標のラベル
        string freqLabel = "Frequency (hertz)";
        mTextureFont->drawString( freqLabel, Vec2f( getWindowCenter().x - mTextureFont->measureString( freqLabel ).x / 2, (float)getWindowHeight() - 20 ) );
        
        // y座標のラベル
        string dbLabel = "Magnitude (decibels, linear)";
        gl::pushModelView();
        gl::translate( 30, getWindowCenter().y + mTextureFont->measureString( dbLabel ).x / 2 );
        gl::rotate( -90 );
        mTextureFont->drawString( dbLabel, Vec2f::zero() );
        gl::popModelView();
    }
    void drawPrintBinInfo( int mouseX ){
        size_t numBins = mMonitorSpectralNode->getFftSize() / 2;
        size_t bin = min( numBins - 1, size_t( ( numBins * ( mouseX - mSpectrumPlot.getBounds().x1 ) ) / mSpectrumPlot.getBounds().getWidth() ) );
        
//        float binFreqWidth = mMonitorSpectralNode->getFreqForBin( 1 ) - mMonitorSpectralNode->getFreqForBin( 0 );
//        float freq = mMonitorSpectralNode->getFreqForBin( bin );
//        float mag = cinder::audio::linearToDecibel( mMagSpectrum[bin] );
//        
//        console() << "bin: " << bin << ", freqency (hertz): " << freq << " - " << freq + binFreqWidth << ", magnitude (decibels): " << mag << endl;
        
    }
    */
    // テクスチャの描画
    void drawTexture(int x, int y){
        
        if( tbox0 ) {
            gl::pushMatrices();
            gl::translate( x, y);//位置
            gl::draw( tbox0 );//描く
            gl::popMatrices();
        }
        
    }
    
    // GL_LIGHT0の色を変える
    void setDiffuseColor( ci::ColorA diffuseColor ){
        gl::color( diffuseColor );
        glMaterialfv( GL_FRONT, GL_DIFFUSE, diffuseColor );
    }
    
    //ウィンドウサイズ
    static const int WindowWidth = 1440;
    static const int WindowHeight = 900;
    
    // カメラ
    CameraPersp  mCam;
    MayaCamUI    mMayaCam;
    
    // パラメータ表示用のテクスチャ（メッセージを表示する）
    gl::Texture mTextTexture0;//パラメーター表示用
    //メッセージリスト表示
    gl::Texture tbox0;//大黒柱
    
    //バックグラウンド
    gl::Texture backgroundImage;
    
    //フォント
    Font mFont;
    
    
    float mRotateMatrix0;//親指（向かって右足）の回転
    float mRotateMatrix2;//人さし指（向かって右腕）の回転
    float mRotateMatrix3;//中指（頭）の回転
    float mRotateMatrix4;//薬指（向かって左腕）の回転
    float mRotateMatrix5;//小指（向かって左足）の回転
    
    
    //パラメーター表示する時に使う
    params::InterfaceGl mParams;

    
    //マリオネットのための変数
    float mTotalMotionScale = 1.0f;//拡大縮小（顔）
    float mTotalMotionScale2 = 1.0f;//拡大縮小（表情）
    
    //ci::Vec3f defFaceTrans(new Point3D(0.0, 120.0, 50.0));
    float defFaceTransX = 1080.0;//顔のx座標の位置
    float defFaceTransY = 675+110.0;//顔のy座標の位置
    float defFaceTransZ = 0.0;//顔のz座標の位置
    
    float defBodyTransX = 1080.0;//体のx座標の位置
    float defBodyTransY = 675.0;//体のy座標の位置
    float defBodyTransZ = 0.0;//体のz座標の位置
    
    float defLeftArmTransX=1080.0+75.0;
    float defRightArmTransX=1080.0-75.0;
    float defArmTransY=675+20.0;
    float defArmTransZ=0.0;

    float rightEyeAngle = 0.0;//右目の角度
    float leftEyeAngle = 0.0;//左目の角度
    float defEyeTransX = 20.0;//右目のx座標の位置
    float defEyeTransY = 120.0;//右目のy座標の位置
    float defEyeTransZ = 0.0;//左目のz座標の位置
    
    float defMouseTransZ = 0.0;//口のz座標の位置
    ci::Vec3f mTotalMotionTranslation;//移動
    
    //メッセージを取得する時に使う
    int messageNumber = -1;

    Paint mPaint;
    Vec2i pointt;//グラフを描写するための座標
    
    float x, y;  //x, y座標
    float A;  //振幅
    float w;  //角周波数
    float p;  //初期位相
    float t;  //経過時間
    float speed1 = 1.0;    //アニメーションの基準となるスピード
    float speed2 = 1.0;
    float eSize = 0.0;
    //sinグラフ
    float t1;  //静止画用経過時間（X座標）
    float t2;  //アニメーション用経過時間（X座標）
    float speed = 1.0;    //アニメーションのスピード
    
    
    Leap::Controller mLeap;
    Leap::Frame mCurrentFrame;//現在のフレーム
    Leap::Frame mLastFrame;//最新のフレーム
    Leap::Hand hand;
    
    
    
    // declare our variables
    Capture	        mCapture;
    gl::Texture		imgTexture;
    
   
    // CAMERA Controls
    CameraPersp mCamPrep;
    Quatf				mSceneRotation;
    float				mCameraDistance;
    Vec3f				mEye, mCenter, mUp;
    
    //音声解析に必要な変数
    audio::InputDeviceNodeRef		mInputDeviceNode;
    audio::MonitorSpectralNodeRef	mMonitorSpectralNode;
    vector<float>					mMagSpectrum;
    
    SpectrumPlot					mSpectrumPlot;
    gl::TextureFontRef				mTextureFont;
    
    //タイマー
    time_t last = time(0);
    time_t next;
    int pastSec = 0;
    
    //Boxのための変数
    float mLeft = 0.0;//左角のx座標
    float mRight = 1440.0;//右角のx座標
    float mTop = 900.0;//上面のy座標
    float mBottom = 0.0;//下面のy座標
    float mBackSide = 500.0;//前面のz座標
    float mFrontSide = -500.0;//後面のz座標
    
};
CINDER_APP_NATIVE( LeapApp, RendererGl )

void setupSocketSv(){
    //ソケットの接続準備まで
    sockfd = ::socket(AF_INET, SOCK_STREAM, 0);//ソケットの生成
    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 9999;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (::bind(sockfd, (struct sockaddr *) &serv_addr,
               sizeof(serv_addr)) < 0)
        error("ERROR on binding"); //socketの登録
    listen(sockfd,5);//ソケット接続準備
    clilen = sizeof(cli_addr);
}
void socketSv(){
    newsockfd = accept(sockfd,
                       (struct sockaddr *) &cli_addr,
                       &clilen);//socketの接続待機（接続要求）
    if (newsockfd < 0)
        error("ERROR on accept");
    bzero(handNumBuff,256);

    l = read(newsockfd,buffer,255);//手の数を読み込む
    if (l < 0){
        error("ERROR reading from socket");
    }
    printf("受け取ったもの: %s\n",buffer);
    /* 1回目の呼出し */
    hans = strtok(buffer, ",");
    /* 2回目以降の呼出し */
    jes1 = strtok(NULL, ",");
    jes2 = strtok(NULL, ",");
    message = strtok(NULL, ",");
    printf("hans: %s\n", hans);
    printf("jes1: %s\n", jes1);
    printf("jes2: %s\n", jes2);
    printf("message: %s\n", message);
    
    
    l = write(newsockfd,"I got your message",10);
    if (l < 0) error("ERROR writing to socket");
    close(newsockfd);
    
}

void *socketSv_loop(void *p){
    setupSocketSv();
    //データを受信する
    while(true){
        socketSv();
    }
}

void readCalculations(){
    //受け取った値を計算する
}
