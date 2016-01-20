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

#include "../include/Resources.h"

#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//音声解析
#include "cinder/gl/TextureFont.h"
#include "cinder/audio/Context.h"
#include "cinder/audio/MonitorNode.h"
#include "../common/AudioDrawUtils.h"

//OBJロード
#include "cinder/ObjLoader.h"
#include "cinder/gl/Vbo.h"


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

#define MAXPOINTS 1300//記録できる点の限度
#define MAXCLIENTNUMBER 7//通信できるクライアントの人数
GLint point[MAXPOINTS][2];//点の座標の入れ物
//std::vector<std::vector<int>> point;//点の座標の入れ物

//サーバー側のソケット生成をする関数
int sockfd, newsockfd, portno = 9999;
socklen_t clilen;
char buffer[256], buffer2[256];
//int clientNumber[MAXCLIENTNUMBER];//クライアントの配列
struct sockaddr_in serv_addr, cli_addr;
int sockaddr_in_size = sizeof(struct sockaddr_in);
int l;//messageの判定に使う
char *account, *hans, *jes1, *jes2, *message;//受け取ったbufferの中のものを分解するときに使う
int handCount=0;//手の数をカウントしたときのカウント
int tapCount=0;//ジェスチャーしたときのカウント
int cirCount=0;//サークルしたときのカウント数
int messageNumber;//受け取ったメッセージの番号
int accountNumber;//受け取ったbufferの中のアカウントナンバー(int型)

char sepMessage[7];
char *separateAccount, *separateHans, *separateJes1, *separateJes2, *separateMessageNumber;
int sepAccount, sepHans, sepJes1, sepJes2, sepMessageNumber;

std::vector<string> saveMessage;

typedef struct{
    time_t time;
    int flag = 0;
    int count[5];
    char msg[256];
} messageInfo;
messageInfo allMessage[7];

string messageList[] = {
    {"大きな声で"},
    {"頑張れ！"},
    {"もう一度説明して"},
    {"面白い！"},
    {"トイレに行きたい"},
    {"わかった"},
    {"かっこいい！"},
    {"ゆっくり話して"},
    {"わからない"},
};

void error(const char *msg){
    //エラーメッセージ
    perror(msg);
    exit(0);
}

void *socketSv_loop(void *p);//socket通信
void setMessage(messageInfo *m, char *data);//メッセージ
messageInfo createMessage(char *data);
int sumOfFrag();
void separateMessage();
void debag(int number);
int sumHands();
int sumJes1();
int sumJes2();
int countMessageNumber();


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
        mFont = Font( "YuGothic", 32 );
        mFontColor = ColorA(0.65, 0.83, 0.58);//文字の色
        
        // カメラ(視点)の設定
        mCameraDistance = 1500.0f;//カメラの距離（z座標）
        mEye			= Vec3f( getWindowWidth()/2, getWindowHeight()/2, mCameraDistance );//位置
        mCenter			= Vec3f( getWindowWidth()/2, getWindowHeight()/2, 0.0f);//カメラのみる先
        //mUp				= Vec3f::yAxis();//頭の方向を表すベクトル
        mCam.setEyePoint( mEye );//カメラの位置
        mCam.setCenterOfInterestPoint(mCenter);//カメラのみる先
        mCam.setPerspective( 45.0f, getWindowAspectRatio(), 300.0f, 3000.0f );//カメラから見える視界の設定
        
        mMayaCam.setCurrentCam(mCam);

        
        // アルファブレンディングを有効にする
        //gl::enableAlphaBlending();
 
        // 描画時に奥行きの考慮を有効にする
//        gl::enableDepthRead();
//        gl::enableDepthWrite();
        
        //スレッドを作る
        pthread_t threadSoc;
        pthread_create(&threadSoc, NULL, socketSv_loop, NULL);
        
        
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
        
        
        // SETUP PARAMS
        mParams = params::InterfaceGl( "Params", Vec2i( 200, 160 ) );//名前、サイズ
        mParams.addParam( "Scene time",  &defArmTransY);
        
        //テクスチャのロード
        objTexture = gl::Texture( loadImage( loadResource( DUMMY_IMAGE ) ) );
        objTexture.enableAndBind();
        //OBJファイルのロード
        ObjLoader loader( (DataSourceRef)loadResource( DUMMY_OBJ ) );
        loader.load( &mMesh );
        mVBO = gl::VboMesh( mMesh );
        
        ObjLoader headLoader( (DataSourceRef)loadResource( HEAD_OBJ ) );
        headLoader.load( &mHead );
        vHead = gl::VboMesh( mHead );
        
        ObjLoader arm2Loader( (DataSourceRef)loadResource( ARM2_OBJ ) );
        arm2Loader.load( &mArm2 );
        vArm2 = gl::VboMesh( mArm2 );
        ObjLoader arm1Loader( (DataSourceRef)loadResource( ARM1_OBJ ) );
        arm1Loader.load( &mArm1 );
        vArm1 = gl::VboMesh( mArm1 );
        ObjLoader handLoader( (DataSourceRef)loadResource( HAND_OBJ ) );
        handLoader.load( &mHand );
        vHand = gl::VboMesh( mHand );
        
        ObjLoader leftArm2Loader( (DataSourceRef)loadResource( LEFT_ARM2_OBJ ) );
        leftArm2Loader.load( &mLeftArm2 );
        vLeftArm2 = gl::VboMesh( mLeftArm2 );
        ObjLoader leftArm1Loader( (DataSourceRef)loadResource( LEFT_ARM1_OBJ ) );
        leftArm1Loader.load( &mLeftArm1 );
        vLeftArm1 = gl::VboMesh( mLeftArm1 );
        ObjLoader leftHandLoader( (DataSourceRef)loadResource( LEFT_HAND_OBJ ) );
        leftHandLoader.load( &mLeftHand );
        vLeftHand = gl::VboMesh( mLeftHand );
        
        ObjLoader bodyLoader( (DataSourceRef)loadResource( BODY_OBJ ) );
        bodyLoader.load( &mBody );
        vBody = gl::VboMesh( mBody );
        ObjLoader footLoader( (DataSourceRef)loadResource( FOOT_OBJ ) );
        footLoader.load( &mFoot );
        vFoot = gl::VboMesh( mFoot );
        
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
//        //カメラのアップデート処理
//        mEye = Vec3f( 0.0f, 0.0f, mCameraDistance );//距離を変える
//        mCamPrep.lookAt( mEye, mCenter, mUp);//カメラの位置、m詰めている先の位置、カメラの頭の方向を表すベクトル
//        gl::setMatrices( mCamPrep );
//        gl::rotate( mSceneRotation );//カメラの回転
        

        
        //音声解析のアップデート処理
        mSpectrumPlot.setBounds( Rectf( 40, 40, (float)getWindowWidth() - 40, (float)getWindowHeight() - 40 ) );
        
        //アップデートごとに一度、メインスレッド上でノードから振幅スペクトルをコピーします。
        mMagSpectrum = mMonitorSpectralNode->getMagSpectrum();
        
        graphUpdate();
        //周回グラフのアップデート
        //R = R + sumJes2();
    }
    
    //描写処理
    void draw(){
        gl::clear();
        gl::enableAdditiveBlending();//PNG画像のエッジがなくす
        //drawBackgroundColor();
        //gl::setMatrices( mMayaCam.getCamera() );
        //"title"描写
        gl::pushMatrices();
        gl::drawString("Server Program", Vec2f(100,100),mFontColor, mFont);
        gl::popMatrices();
        gl::pushMatrices();
            drawListArea();//メッセージリストの表示
            drawCircle();//サークルで表示
            drawCircle2();//サークルで表示
            drawCircle3();//サークルで表示
            //drawPainting();//指の軌跡を描く
            //drawAudioAnalyze();//音声解析の描写
            //drawSinGraph();//sinグラフを描く
            drawBarGraph();
            drawAccessNumber();
        
            //drawAccessNumber();
        gl::popMatrices();

        //アクセス数に応じてマリオネットを表示
        for(int i = 0; i < sumOfFrag(); i++){
            gl::pushMatrices();
            translate(Vec2d(i*200,0));
            drawObjFile();
            gl::popMatrices();
        }
        //drawObjFile();
        //mParams.draw();
    }
    //背景色を変える
    void drawBackgroundColor(){
        int largeMessageNumber = countMessageNumber();//一番多かったメッセージ番号を取ってくる
        if(largeMessageNumber == 1){
            glClearColor(1.0f, 0.0f, 0.0f, 1.0f);//赤
        }else if(largeMessageNumber == 2){
            glClearColor(0.0f, 1.0f, 0.0f, 1.0f);//青
        }else if(largeMessageNumber == 3){
            glClearColor(0.0f, 0.0f, 1.0f, 1.0f);//緑
        }else if(largeMessageNumber == 4){
            glClearColor(1.0f, 1.0f, 0.0f, 1.0f);//黄
        }else if(largeMessageNumber == 5){
            glClearColor(0.0f, 1.0f, 1.0f, 1.0f);//黄緑？
        }else if(largeMessageNumber == 6){
            glClearColor(1.0f, 0.0f, 1.0f, 1.0f);//紫
        }else if(largeMessageNumber == 7){
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        }else if(largeMessageNumber == 8){
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        }else if(largeMessageNumber == 9){
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        }else{
        
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
            glTranslatef(defBodyTransX+25,defBodyTransY+75,defBodyTransZ);//移動
            glRotatef(mRotateMatrix0, 1.0f, 0.0f, 0.0f);//回転
            glScalef( mTotalMotionScale/4, mTotalMotionScale/2, mTotalMotionScale/2 );//大きさ
            gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 100, 100, 100 ) );//実体
        gl::popMatrices();
        
        //左足を描く
        gl::pushMatrices();
            setDiffuseColor( ci::ColorA( 0.7f, 0.7f, 0.7f, 1.0f ) );
            glTranslatef(defBodyTransX-25,defBodyTransY+75,defBodyTransZ);//移動
            glRotatef(mRotateMatrix5, 1.0f, 0.0f, 0.0f);//回転
            glScalef( mTotalMotionScale/4, mTotalMotionScale/2, mTotalMotionScale/2 );//大きさ
            gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 100, 100, 100 ) );//実体
        gl::popMatrices();
        
    }
    //マリオネット
    void drawObjFile(){
        
        glDisable( GL_CULL_FACE );//ポリゴンの表面だけを描く
        
        gl::pushMatrices();
        objTexture.bind();
        gl::translate(Vec3f(defFaceTransX,defFaceTransY,defFaceTransZ));
        gl::draw( mHead );
        gl::popMatrices();
        
        gl::pushMatrices();
        objTexture.bind();
        gl::translate(Vec3f(defRightArmTransX,defArmTransY,defArmTransZ));
        gl::draw( mArm2 );
        gl::popMatrices();gl::pushMatrices();

        gl::pushMatrices();
        objTexture.bind();
        gl::translate(Vec3f(defRightArmTransX,defArmTransY,defArmTransZ));
        gl::draw( mArm1 );
        gl::popMatrices();gl::pushMatrices();

        gl::pushMatrices();
        objTexture.bind();
        gl::translate(Vec3f(defRightArmTransX,defArmTransY,defArmTransZ));
        gl::draw( mHand );
        gl::popMatrices();
        
        gl::pushMatrices();
        objTexture.bind();
        gl::translate(Vec3f(defLeftArmTransX,defArmTransY,defArmTransZ));
        gl::draw( mLeftArm2 );
        gl::popMatrices();
        
        gl::pushMatrices();
        gl::translate(Vec3f(defLeftArmTransX,defArmTransY,defArmTransZ));
        gl::draw( mLeftArm1 );
        gl::popMatrices();
        
        gl::pushMatrices();
        objTexture.bind();
        gl::translate(Vec3f(defLeftArmTransX,defArmTransY,defArmTransZ));
        gl::draw( mLeftHand );
        gl::popMatrices();
        
        objTexture.bind();
        gl::translate(Vec3f(defBodyTransX,defBodyTransY,defBodyTransZ));
        gl::draw( mBody );
        gl::popMatrices();
        
        gl::pushMatrices();
        objTexture.bind();
        gl::translate(Vec3f(defBodyTransX,defBodyTransY,defBodyTransZ));
        gl::draw( mFoot );
        gl::popMatrices();
    }
    
    //ScreenTapの回数によって大きくなる円の描写
    void drawCircle(){
        //ScreenTapの回数によって大きくなる円の描写
        int handRadius = sumJes2();
        int rad = (R + handRadius);
        setDiffuseColor( ci::ColorA(0.65, 0.83, 0.58));
        gl::pushMatrices();
        gl::drawString("この値はScreenTapの回数によって大きくなる円のhandRadius"+toString(handRadius), Vec2d( 360, 100));
        gl::drawSolidCircle(Vec2d( 360, WindowHeight/2 ), rad * 8);//ジェスチャーによって円の半径が変わる
        gl::popMatrices();
        setDiffuseColor( ci::ColorA( 0.8, 0.8, 0.8 ) );

    }
    //Circleジェスチャーによって移動する円の描写
    void drawCircle2(){
        //円の周回運動
        int handSpeed = sumJes1();//サークルジェスチャーの回数
        int handRadius = sumJes2();//スクリーンタップジェスチャーの回数
        int rad = (R + handRadius);
        //float theta = angle * PI /180;  //thetaは角度（angle）をラディアン値に直したもの
        float theta = handSpeed * PI /180;  //thetaは角度（handSpeed）をラディアン値に直したもの
        setDiffuseColor( ci::ColorA(0.65, 0.83, 0.58));
        gl::pushMatrices();
        gl::drawString("この値はCircleジェスチャーによって移動する円のR"+toString(R), Vec2d( 360, 800));
        //円を描く
        //gl::drawStrokedCircle(Vec2d( 360, WindowHeight/2 ), R * 20);//ジェスチャーによって円の半径が変わる//テスト用
        gl::drawStrokedCircle(Vec2d( 360, WindowHeight/2 ), rad * 10);//ジェスチャーによって円の半径が変わる
        //円の周りを動く円のアニメーション
//        posX = R * 20 * cos(theta);//テスト用
//        posY = -R * 20 * sin(theta);//テスト用
        posX = rad * 10 * cos(theta);
        posY = -rad * 10 * sin(theta);
        gl::drawSolidCircle(Vec2d( posX + 360, posY + WindowHeight/2 ), 10);
        //angle ++;//テスト用
        //R = R + 1;//テスト用
//        if (angle >= 360) angle = 0;  //もしangleが360以上になったら0にする。//テスト用
        if (handSpeed >= 360) handSpeed = 0;  //もしangleが360以上になったら0にする。
//        if (R * 10 > 400) R = 10;//テスト用
        if ((R + handRadius) * 10 > 400) rad = 400;
        gl::popMatrices();
        setDiffuseColor( ci::ColorA( 0.8, 0.8, 0.8 ) );
    }
    //説明用の円
    void drawCircle3(){
    //説明用の円
        gl::pushMatrices();
        gl::drawSphere(Vec3d( 360, 675, 0 ), 10);//指の位置
        gl::popMatrices();
        setDiffuseColor( ci::ColorA( 0.8, 0.8, 0.8 ) );
        
    }
    
    //メッセージリスト
    void drawListArea(){
        
        //"大きな声で"描写
        gl::pushMatrices();
        gl::drawString(messageList[0],Vec2f(992.5, 145), mFontColor, mFont);
        gl::translate(Vec2f(992.5, 145));
        drawBox();
        gl::popMatrices();
        
        //"頑張れ"描写
        gl::pushMatrices();
        gl::drawString(messageList[1],Vec2f(992.5, 215), mFontColor, mFont);
        gl::translate(Vec2f(992.5, 215));
        drawBox();
        gl::popMatrices();
        
        //"もう一度説明して"描写
        gl::pushMatrices();
        gl::drawString(messageList[2],Vec2f(992.5, 285), mFontColor, mFont);
        gl::translate(Vec2f(992.5, 285));
        drawBox();
        gl::popMatrices();
        
        //"面白い"描写
        gl::pushMatrices();
        gl::drawString(messageList[3],Vec2f(992.5, 355), mFontColor, mFont);
        gl::translate(Vec2f(992.5, 355));
        drawBox();
        gl::popMatrices();
        
        //"トイレにいきたい"描写
        gl::pushMatrices();
        gl::drawString(messageList[4],Vec2f(992.5, 425), mFontColor, mFont);
        gl::translate(Vec2f(992.5, 425));
        drawBox();
        gl::popMatrices();
        
        //"わかった"描写
        gl::pushMatrices();
        gl::drawString(messageList[5],Vec2f(992.5, 495), mFontColor, mFont);
        gl::translate(Vec2f(992.5, 495));
        drawBox();
        gl::popMatrices();
        
        //"かっこいい"描写
        gl::pushMatrices();
        gl::drawString(messageList[6],Vec2f(992.5, 565), mFontColor, mFont);
        gl::translate(Vec2f(992.5, 565));
        drawBox();
        gl::popMatrices();
        
        //"速い!"描写
        gl::pushMatrices();
        gl::drawString(messageList[7],Vec2f(992.5, 635), mFontColor, mFont);
        gl::translate(Vec2f(992.5, 635));
        drawBox();
        gl::popMatrices();
        
        //"わからない"描写
        gl::pushMatrices();
        gl::drawString(messageList[8],Vec2f(992.5, 705), mFontColor, mFont);
        gl::translate(Vec2f(992.5, 705));
        drawBox();
        gl::popMatrices();
    }
    
    //枠としてのBoxを描く
    void drawBox(){
        setDiffuseColor( ci::ColorA(0.65, 0.83, 0.58));
        gl::drawStrokedRoundedRect(Rectf(0,0,270,50), 5);//角の丸い四角
        setDiffuseColor( ci::ColorA( 0.8, 0.8, 0.8 ) );
    }
    
    void drawAccessNumber(){
        
        int h = sumHands();
        int j1 = sumJes1();
        int j2 = sumJes2();
        int cmn = countMessageNumber();
        
        gl::drawString("トータルアクセス数", Vec2d(1200,700));
        gl::drawString(to_string(sumOfFrag()), Vec2d(1200,800));
        gl::drawString("一番多いメッセージナンバー", Vec2d(1200,100));
        gl::drawString(to_string(cmn), Vec2d(1200,110));
        gl::drawString("手の数", Vec2d(1200,150));
        gl::drawString(to_string(h), Vec2d(1200,160));
        gl::drawString("サークル数", Vec2d(1200,180));
        gl::drawString(to_string(j1), Vec2d(1200,200));
        gl::drawString("タップ数", Vec2d(1200,210));
        gl::drawString(to_string(j2), Vec2d(1200,230));
    }
    //お絵かき（手の軌跡を描写する）
    void drawPainting(){
        
        // 表示座標系の保持
        gl::pushMatrices();
        // 描画
        mPaint.draw();
        gl::popMatrices();
        
    }
    
    //sinグラフを描く
    void drawSinGraph(){
        
        glPushMatrix();
       // gl::setMatrices( mMayaCam.getCamera() );
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
        //gl::setMatrices( mMayaCam.getCamera() );
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
//            printf("%d 秒経過\n", pastSec);
            point[pastSec][0]=pastSec;
            point[pastSec][1]=handCount;
            
//            std::cout << "graphUpdate関数"<<"\n"
//            << "秒数：" << pastSec << "\n"
//            << "手の数：" << handCount << "\n"
//            << std::endl;
            
        }
    }
    //棒グラフを描く
    void drawBarGraph(){
        for (int i = 0; i < pastSec; i++) {
            //棒グラフを描写していく
//            glPushMatrix();
//                glBegin(GL_LINE_STRIP);
//                glColor3d(1.0, 0.0, 0.0);
//                glLineWidth(10);
//                glVertex2d(point[i][0]*10, 0);//x座標
//                glVertex2d(point[i][0]*10 , point[i][1]*100);//y座標
//                glEnd();
//            glPopMatrix();
            glPushMatrix();
            gl::drawCube(Vec3f(point[i][0]*10,0,0), Vec3f(10,point[i][1]*100,10));
            glPopMatrix();
            
        }
    }
    
    //音声解析の描写
    void drawAudioAnalyze(){
        glPushMatrix();
        glScalef(0.5, 0.5, 0);
        mSpectrumPlot.draw( mMagSpectrum );
        drawLabels();
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
    Color mFontColor;
    
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
    float defFaceTransX = 200.0;//顔のx座標の位置
    float defFaceTransY = 550.0;//顔のy座標の位置
    float defFaceTransZ = 0.0;//顔のz座標の位置
    
    float defBodyTransX = 200.0;//体のx座標の位置
    float defBodyTransY = 550.0;//体のy座標の位置
    float defBodyTransZ = 0.0;//体のz座標の位置
    
    float defLeftArmTransX=200.0;
    float defRightArmTransX=200.0;
    float defArmTransY=550.0;
    float defArmTransZ=0.0;
    
    float rightEyeAngle = 0.0;//右目の角度
    float leftEyeAngle = 0.0;//左目の角度
    float defEyeTransX = 20.0;//右目のx座標の位置
    float defEyeTransY = 120.0;//右目のy座標の位置
    float defEyeTransZ = 0.0;//左目のz座標の位置
    
    float defMouseTransZ = 0.0;//口のz座標の位置
    
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
    
    //周回sinグラフ
    int posX, posY;//移動する円の位置
    int R = 5;  //軌跡を描く円の半径（初期値を100）
    int angle = 0;  //角度
    
    
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
    
    //OBJファイルを表示するための変数
    TriMesh			mMesh;
    gl::VboMesh		mVBO;
    gl::Texture		objTexture;
    
    TriMesh			mHead;
    gl::VboMesh		vHead;
    
    TriMesh			mArm2;
    gl::VboMesh		vArm2;
    
    TriMesh			mArm1;
    gl::VboMesh		vArm1;
    
    TriMesh			mHand;
    gl::VboMesh		vHand;
    
    TriMesh			mLeftArm2;
    gl::VboMesh		vLeftArm2;
    
    TriMesh			mLeftArm1;
    gl::VboMesh		vLeftArm1;
    
    TriMesh			mLeftHand;
    gl::VboMesh		vLeftHand;
    
    TriMesh			mBody;
    gl::VboMesh		vBody;
    
    TriMesh			mFoot;
    gl::VboMesh		vFoot;
    
    
};
CINDER_APP_NATIVE( LeapApp, RendererGl )

void setupSocketSv(){
    //ソケットの接続準備まで
    sockfd = ::socket(AF_INET, SOCK_STREAM, 0);//ソケットの生成
    //ソケットが生成されていない
    if (sockfd < 0)
        error("ERROR opening socket");
    
    //ホスト情報を構造体にセット
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    //socketにアドレスとポート番号とバインド
    if (::bind(sockfd, (struct sockaddr *) &serv_addr,
               sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    
    listen(sockfd,5);//ソケット接続準備(6個まで)
}

void socketSv(){
    messageInfo msgInfo;
    
    clilen = sizeof(cli_addr);//クライアントから接続がくるまで待ち、接続がくるとこの関数を抜ける
    //socketの接続待機（接続要求）
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);//ソケット番号を指定、相手の情報を格納するためのsockaddr_in型の構造体へのポインターを渡す
    //接続失敗
    if (newsockfd < 0)
        error("ERROR on accept");
    
    bzero(buffer,256);
    l = read(newsockfd,buffer,255);//クライアント側からデータを受信する
    
    //データが読み込めなかった
    if (l < 0){
        error("ERROR reading from socket");
    }

    //受けとったものをプリント
    printf("受け取ったもの: %s\n",buffer);
    strcpy(buffer2, buffer);
    
    //以下は受けとったものをカンマで分解する
    accountNumber = atoi(strtok(buffer2, ",")/*クライアントの番号*/);//int型に変換
    
    msgInfo = createMessage(buffer); //受け取ったメッセージを構造体の配列に記録
    if (msgInfo.count[4] == -1) {
        //msgInfo構造体count[4]の値（メッセージの番号）が-1のときはなにもしない
    } else {
        allMessage[accountNumber] = msgInfo;//それ以外のときはallMessageに記録
    }
    debag(accountNumber);//記録したものをデバッグする
    
    //メッセージが受け取れていることをクライアント側に発信
    l = write(newsockfd,"I got your message",18);
    //発信が失敗
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

messageInfo createMessage(char *data){
    messageInfo msg;
    char m[256];
    
    //時間を保存する
    msg.time = time(NULL);
    
    //受け取ったmessageを保存していく
    strcpy(msg.msg, data);
    
    //受け取りフラグ
    msg.flag = 1;
    
    /* 1回目の呼出し */
    strcpy(m, data);
        
    msg.count[0] = atoi(strtok(m, ",")/*クライアントの番号*/);//int型に変換
    for (int i = 1; i < 5; i++) {
       msg.count[i] = atoi(strtok(NULL, ","));
    }
    return msg;
}

//アクセス数を返す関数
int sumOfFrag(){
    
    int sum =0;
    for (int i = 0; i < 7; i++) {
        sum = sum + allMessage[i].flag;
    }
    return sum;
}

//送られてきた手の数を集計して、合計した値を返す関数
int sumHands(){
    int hands = 0;
    for (int i = 0; i < 7 ; i++) {
        if (allMessage[i].flag == 1) {
            hands = hands + allMessage[i].count[1];
        }
    }
    return hands;
}

//送られてきたジェスチャーの数を集計して、合計した値を返す関数
int sumJes1(){
    int jes1 = 0;
    for (int i = 0; i < 7 ; i++) {
        if (allMessage[i].flag == 1) {
            jes1 = jes1 + allMessage[i].count[2];
        }
    }
    return jes1;
}

//送られてきたジェスチャーの数を集計して、合計した値を返す関数
int sumJes2(){
    int jes2 = 0;
    for (int i = 0; i < 7 ; i++) {
        if (allMessage[i].flag == 1) {
            jes2 = jes2 + allMessage[i].count[3];
        }
    }
    return jes2;
}


//送られてきたメッセージの中でどれが多いかを算出し、返す関数
int countMessageNumber(){
    int sumMessageNumber[9] = {0,0,0,0,0,0,0,0,0};//初期化
    int messageNumber = 0;//返す値
    //記録しているぶんのmessageの値を参照して、その要素に加えていく
    for (int i = 0; i < 7; i++) {
        if (allMessage[i].flag == 1) {
            if (allMessage[i].count[4]=='0') {
                sumMessageNumber[0]++;
            }
            else if (allMessage[i].count[4]=='1') {
                sumMessageNumber[1]++;
            }
            else if (allMessage[i].count[4]=='2') {
                sumMessageNumber[2]++;
            }
            else if (allMessage[i].count[4]=='3') {
                sumMessageNumber[3]++;
            }
            else if (allMessage[i].count[4]=='4') {
                sumMessageNumber[4]++;
            }
            else if (allMessage[i].count[4]=='5') {
                sumMessageNumber[5]++;
            }
            else if (allMessage[i].count[4]=='6') {
                sumMessageNumber[6]++;
            }
            else if (allMessage[i].count[4]=='7') {
                sumMessageNumber[7]++;
            }
            else if (allMessage[i].count[4]=='8') {
                sumMessageNumber[8]++;
            }
        }
    }
    //要素数が最大の番地を求める
    for(int j = 0; j < sizeof(sumMessageNumber); j++){
        if(messageNumber < sumMessageNumber[j]){
            messageNumber = j;
        }
    }
    return messageNumber;
}

//構造体の中身を確認するための関数
void debag(int number){
    //送られてきたアカウント名のデータをプリントする
        printf("時間：%ld\n", allMessage[number].time);
        printf("フラグ：%d\n", allMessage[number].flag );
        printf("count[0]の値：%d\n", allMessage[number].count[0]);
        printf("count[1]の値：%d\n", allMessage[number].count[1]);
        printf("count[2]の値：%d\n", allMessage[number].count[2]);
        printf("count[3]の値：%d\n", allMessage[number].count[3]);
        printf("count[4]の値：%d\n", allMessage[number].count[4]);
}
