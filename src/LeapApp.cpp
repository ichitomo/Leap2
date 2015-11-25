#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"


#include "cinder/gl/Texture.h"//画像を描画させたいときに使う
#include "cinder/Text.h"
#include "cinder/MayaCamUI.h"
#include "Leap.h"
#include "cinder/params/Params.h"//パラメーターを動的に扱える
#include "cinder/ImageIo.h"//画像を描画させたいときに使う
#include "cinder/ObjLoader.h"//
#include "cinder/Utilities.h"

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

struct Messages_base{
    char message[40];
};

Messages_base messageList[] = {
    
    {"わかった"},
    {"わからない"},
    {"もう一度"},
    {"戻って"},
    {"速い"},
    {"大きな声で"},
    {"面白い"},
    {"いいね！"},
    {"すごい！"},
    {"かっこいい"},
    {"なるほど"},
    {"ありがとうございます"},
    {"うわー"},
    {"楽しみ"},
    {"はい"},
    {"いいえ"},
    {"きたー"},
    {"トイレに行きたいです"},
};

void error(const char *msg){
    //エラーメッセージ
    perror(msg);
    exit(0);
}

//サーバー側のソケット生成をする関数
int sockfd, newsockfd, portno;
socklen_t clilen;
char buffer[256];
struct sockaddr_in serv_addr, cli_addr;
int n;

void *socketSv_loop(void *p);

class LeapApp : public AppNative {
    
public:
    
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
        mCam.setEyePoint( Vec3f( 0.0f, 150.0f, 500.0f ) );//カメラの位置
        mCam.setCenterOfInterestPoint( Vec3f( 0.0f, 0.0f, 1.0f ) );//カメラの中心座標
        mCam.setPerspective( 45.0f, getWindowAspectRatio(), 50.0f, 3000.0f );//カメラから見える視界の設定
        
        mMayaCam.setCurrentCam(mCam);
        
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
    }
    void setupSocketSv();
    void socketSv();
    
    //描写処理
    void *draw(void *p){
        gl::clear();
        gl::pushMatrices();// カメラ位置を設定する
        //gl::setMatrices( mMayaCam.getCamera() );
        drawMarionette();//マリオネット描写
        drawListArea();//メッセージリストの表示
        gl::popMatrices();
        // パラメーター設定UIを描画する
        return NULL;
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
        glTranslatef(10.0,0.0f,0.0f);//移動
        gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 100, 80, 100 ) );//実体
        gl::popMatrices();
        
        //右目
        gl::pushMatrices();
        glTranslatef(defEyeTransX,defEyeTransY,defEyeTransZ);//位置
        glRotatef(rightEyeAngle, 1.0f, 0.0f, 0.0f);//回転
        glScalef( mTotalMotionScale2/5, mTotalMotionScale2/10, mTotalMotionScale2/10 );//大きさ
        gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 100, 100, 100 ) );//実体
        gl::popMatrices();
        
        //左目
        gl::pushMatrices();
        glTranslatef(defEyeTransX,defEyeTransY,defEyeTransZ);//位置
        glRotatef(leftEyeAngle, 1.0f, 0.0f, 0.0f);//回転
        glScalef( mTotalMotionScale2/5, mTotalMotionScale2/10, mTotalMotionScale2/10 );//大きさ
        gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 100, 100, 100 ) );//実体
        gl::popMatrices();
        
        //元に戻す
        rightEyeAngle = 0.0;//右目の角度
        leftEyeAngle = 0.0;//左目の角度
        defEyeTransX = 20.0;//右目の角度
        defEyeTransY = 20.0;//右目の角度
        defEyeTransZ = 100.0;//左目の角度
        
        //胴体を描く
        gl::pushMatrices();
        glTranslatef(defBodyTransX,defBodyTransY,defBodyTransZ);//移動
        glScalef( mTotalMotionScale, mTotalMotionScale, mTotalMotionScale );//大きさ
        gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 100, 100, 100 ) );//実体
        gl::popMatrices();
        
        //右腕を描く
        gl::pushMatrices();
        setDiffuseColor( ci::ColorA( 0.7f, 0.7f, 0.7f, 1.0f ) );
        glTranslatef(defBodyTransX+75,defBodyTransY,defBodyTransZ);//移動
        glRotatef(mRotateMatrix2, 1.0f, 1.0f, 0.0f);//回転
        glTranslatef( mTotalMotionTranslation.x/10.0,
                     mTotalMotionTranslation.y/10.0,
                     0.0f);//移動
        glScalef( mTotalMotionScale/2, mTotalMotionScale/4, mTotalMotionScale/2 );//大きさ
        gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 100,  50, 50 ) );//実体
        
        //二個目
        glTranslatef( defBodyTransX+85,0.0f,0.0f);//移動
        glRotatef(mRotateMatrix2, 1.0f, 1.0f, 0.0f);//回転
        glTranslatef(10.0,10.0,0.0f);//移動
        gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 50,  50, 50 ) );//実体
        gl::popMatrices();
        
        //左腕を描く
        gl::pushMatrices();
        setDiffuseColor( ci::ColorA( 0.7f, 0.7f, 0.7f, 1.0f ) );
        glTranslatef(defBodyTransX-75,defBodyTransY,defBodyTransZ);//移動
        glRotatef(-mRotateMatrix4, -1.0f, 1.0f, 0.0f);//回転
        glTranslatef(10.0,10.0,0.0f);//移動
        glScalef( mTotalMotionScale/2, mTotalMotionScale/4, mTotalMotionScale/2 );//大きさ
        gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 100, 50, 50 ) );//実体
        //二個目
        glTranslatef(defBodyTransX-85,0.0f,0.0f);//移動
        glRotatef(-mRotateMatrix4, -1.0f, 1.0f, 0.0f);//回転
        glTranslatef(10.0,10.0,0.0f);//移動
        gl::drawColorCube( Vec3f( 0,0,0 ), Vec3f( 50, 50, 50 ) );//実体
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
        gl::pushMatrices();
        auto tbox0 = TextBox().alignment( TextBox::LEFT ).font( mFont ).text ( buffer ).color(Color( 1.0f, 1.0f, 1.0f )).backgroundColor( ColorA( 0, 1.0f, 0, 0 ) );
        auto mTextTexture = gl::Texture( tbox0.render() );
        gl::draw( mTextTexture );
        gl::popMatrices();
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
    
    //ここ消したらスレッドが止まる！！
    // Leap Motion
    Leap::Controller mLeap;//ジェスチャーの有効化など...
    Leap::Frame mCurrentFrame;//現在
    Leap::Frame mLastFrame;//最新
    
    Leap::Matrix mRotationMatrix;//回転
    Leap::Vector mTotalMotionTranslation;//移動
    //ここまで
    
    float mRotateMatrix0;//親指（向かって右足）の回転
    float mRotateMatrix2;//人さし指（向かって右腕）の回転
    float mRotateMatrix3;//中指（頭）の回転
    float mRotateMatrix4;//薬指（向かって左腕）の回転
    float mRotateMatrix5;//小指（向かって左足）の回転
    
    //ジェスチャーのための変数追加
    Leap::Frame lastFrame;//最後
    
    //ジェスチャーを取得するための変数
    std::list<Leap::Gesture> mGestureList;
    
    
    //各ジェスチャーを使用する
    std::list<Leap::SwipeGesture> swipe;
    std::list<Leap::CircleGesture> circle;
    std::list<Leap::KeyTapGesture> keytap;
    std::list<Leap::ScreenTapGesture> screentap;
    
    //パラメーター表示する時に使う
    params::InterfaceGl mParams;
    
    //ジェスチャーをするときに取得できる変数
    //スワイプ
    float mMinLenagth;//長さ
    float mMinVelocity;//速さ
    //サークル
    float mMinRadius;//半径
    float mMinArc;//弧
    
    //キータップ、スクリーンタップ
    float mMinDownVelocity;//速さ
    float mHistorySeconds;//秒数
    float mMinDistance;//距離
    
    //マリオネットのための変数
    float mTotalMotionScale = 1.0f;//拡大縮小（顔）
    float mTotalMotionScale2 = 1.0f;//拡大縮小（表情）
    
    float defFaceTransX = 0.0;//顔のx座標の位置
    float defFaceTransY = 0.0;//顔のy座標の位置
    float defFaceTransZ = 50.0;//顔のz座標の位置
    float rightEyeAngle = 0.0;//右目の角度
    float leftEyeAngle = 0.0;//左目の角度
    float defEyeTransX = 20.0;//右目のx座標の位置
    float defEyeTransY = 20.0;//右目のy座標の位置
    float defEyeTransZ = 0.0;//左目のz座標の位置
    float defBodyTransX = 0.0;//体のx座標の位置
    float defBodyTransY = -100.0;//体のy座標の位置
    float defBodyTransZ = 50.0;//体のz座標の位置
    float defMouseTransZ = 110.0;//口のz座標の位置
    
    //InteractionBoxの実装
    Leap::InteractionBox iBox;
    
    float mLeft;//左の壁
    float mRight;//右の壁
    float mTop;//雲
    float mBaottom;//底
    float mBackSide;//背面
    float mFrontSide;//正面

    //メッセージを取得する時に使う
    int messageNumber = -1;
};
CINDER_APP_NATIVE( LeapApp, RendererGl )

void setupSocketSv(){
    //ソケットの接続準備まで
    sockfd = ::socket(AF_INET, SOCK_STREAM, 0);//ソケットの生成
    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi("9999");
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
    bzero(buffer,256);
    n = read(newsockfd,buffer,255);
    if (n < 0) error("ERROR reading from socket");
    printf("Here is the message: %s\n",buffer);
    n = write(newsockfd,"I got your message",18);
    if (n < 0) error("ERROR writing to socket");
    close(newsockfd);
}

void *socketSv_loop(void *p){
    setupSocketSv();
    //データを受信する
    while(true){
        socketSv();
    }
}
