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

#include <algorithm>//最大値を求める

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
#define MAXCLIENTNUMBER 6//通信できるクライアントの人数
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
int handCount = 0;//手の数をカウントしたときのカウント
int ktapCount = 0;//ジェスチャーしたときのカウント
int cirCount = 0;//サークルしたときのカウント数
int stapCount = 0;
int swipeCOunt = 0;
int messageNumber;//受け取ったメッセージの番号
int accountNumber;//受け取ったbufferの中のアカウントナンバー(int型)

char sepMessage[6];
char *separateAccount, *separateHans, *separateJes1, *separateJes2, *separateMessageNumber;
int sepAccount, sepHans, sepJes1, sepJes2, sepMessageNumber;
int mcount = 0;
int mcount2 = 0;
int mcount3 = 0;
int mcount4 = 0;
int mcount5 = 0;
int mcount6 = 0;
int mcount7 = 0;
int mcount8 = 0;
int mcount9 = 0;
std::vector<string> saveMessage;

typedef struct{
    time_t time;
    int flag = 0;
    int count[7];
    char msg[256];
} messageInfo;
messageInfo allMessage[6];

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
int sumJes3();
int sumJes4();
int countMessageNumber();
void leapLog(int number);


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
        gl::enableAlphaBlending();
        
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
        
        //テクスチャのロード
        objTexture = gl::Texture( loadImage( loadResource( DUMMY_IMAGE ) ) );
        objTexture.enableAndBind();
        //OBJファイルのロード
        ObjLoader loader( (DataSourceRef)loadResource( DUMMY_OBJ ) );
        loader.load( &mMesh );
        mVBO = gl::VboMesh( mMesh );
    }
    void setupSocketSv();
    void socketSv();
    // マウスのクリック
    void mouseDown( MouseEvent event ){
        mMayaCam.mouseDown( event.getPos() );
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
    }
    
    //描写処理
    void draw(){
        gl::clear();
        //"title"描写
        gl::pushMatrices();
        gl::drawString("Server Program", Vec2f(100,50),mFontColor, mFont);
        gl::popMatrices();
        gl::pushMatrices();
            drawListArea();//メッセージリストの表示
            drawChangeCircleColor();
            drawCircle();//サークルで表示
            drawCircle2();//サークルで表示
            drawHelp();
            setDiffuseColor( ci::ColorA(0.65, 0.83, 0.58));
            drawAccessNumber();
        gl::popMatrices();

        //アクセス数に応じてマリオネットを表示
        for(int i = 0; i < sumOfFrag(); i++){
            gl::pushMatrices();
            translate(Vec2d(i*100,0));
            drawObjFile();
            gl::popMatrices();
        }
    }
    //背景色を変える
    void drawChangeCircleColor(){
        int largeMessageNumber = countMessageNumber();//一番多かったメッセージ番号を取ってくる
        if(largeMessageNumber == 0){
            setDiffuseColor( ci::ColorA(1.0f, 0.0f, 0.0f, 1.0f));//赤
        }else if(largeMessageNumber == 1){
            setDiffuseColor( ci::ColorA(0.0f, 1.0f, 0.0f, 1.0f));//緑
        }else if(largeMessageNumber == 2){
            setDiffuseColor( ci::ColorA(0.1f, 0.0f, 0.85f, 1.0f));//青
        }else if(largeMessageNumber == 3){
            setDiffuseColor( ci::ColorA(1.0f, 1.0f, 0.0f, 1.0f));//黄
        }else if(largeMessageNumber == 4){
            setDiffuseColor( ci::ColorA(0.0f, 1.0f, 1.0f, 1.0f));//水色？
        }else if(largeMessageNumber == 5){
            setDiffuseColor( ci::ColorA(1.0f, 0.0f, 1.0f, 1.0f));//紫
        }else if(largeMessageNumber == 6){
            setDiffuseColor( ci::ColorA(0.53f, 0.0f, 0.85f, 1.0f));
        }else if(largeMessageNumber == 7){
            setDiffuseColor( ci::ColorA(0.5f, 0.5f, 0.5f, 1.0f));
        }else if(largeMessageNumber == 8){
            setDiffuseColor( ci::ColorA(1.0f, 1.0f, 1.0f, 1.0f));
        }else{
            setDiffuseColor( ci::ColorA(0.65, 0.83, 0.58));
        }

    }
    //マリオネット
    void drawObjFile(){
        glDisable( GL_CULL_FACE );//ポリゴンの表面だけを描く
        // 描画時に奥行きの考慮を有効にする
        gl::enableDepthRead();
        gl::enableDepthWrite();
        gl::pushMatrices();
        objTexture.bind();
        gl::translate(Vec3f(200,800,0));
        gl::rotate(Vec3f(180,0,0));
        gl::scale(Vec3f(0.5, 0.5, 0.5));
        gl::draw( mMesh );
        gl::popMatrices();
    }
    
    //ScreenTapの回数によって大きくなる円の描写
    void drawCircle(){
        handRadius = sumJes3() + sumJes4();//スクリーンタップジェスチャーの回数
        rad = (R + handRadius);
        //ScreenTapの回数によって大きくなる円の描写
        gl::pushMatrices();
        gl::drawSolidCircle(Vec2d( 360, WindowHeight/2 ), rad * 8);//ジェスチャーによって円の半径が変わる
        gl::popMatrices();

    }
    //Circleジェスチャーによって移動する円の描写
    void drawCircle2(){
        //円の周回運動
        handSpeed = sumJes1()+ sumJes2();//サークルジェスチャーの回数
        handRadius = sumJes3() + sumJes4();//タップジェスチャーの回数
        rad = (R + handRadius);
        circleSpeed = angle + handSpeed;
        //float theta = angle * PI /180;  //thetaは角度（angle）をラディアン値に直したもの
        float theta = circleSpeed * PI /180;  //thetaは角度（handSpeed）をラディアン値に直したもの
        gl::pushMatrices();
        gl::drawStrokedCircle(Vec2d( 360, WindowHeight/2 ), rad * 10);//ジェスチャーによって円の半径が変わる
        //円の周りを動く円のアニメーション
        posX = rad * 10 * cos(theta);
        posY = -rad * 10 * sin(theta);
        gl::drawSolidCircle(Vec2d( posX + 360, posY + WindowHeight/2 ), 10);
        angle ++;//テスト用
        if (circleSpeed >= 360) circleSpeed = 0;  //もしangleが360以上になったら0にする。
        if (rad * 10 > 400) rad = 400;
        gl::popMatrices();
    }

    //メッセージリスト
    void drawListArea(){
        setDiffuseColor( ci::ColorA(0.65, 0.83, 0.58));
        for(int i = 0; i < 9; i++){
            
            if(i == 0){
                setDiffuseColor( ci::ColorA(1.0f, 0.0f, 0.0f, 1.0f));//赤
            }else if(i == 1){
                setDiffuseColor( ci::ColorA(0.0f, 1.0f, 0.0f, 1.0f));//緑
            }else if(i == 2){
                setDiffuseColor( ci::ColorA(0.1f, 0.0f, 0.85f, 1.0f));//青
            }else if(i == 3){
                setDiffuseColor( ci::ColorA(1.0f, 1.0f, 0.0f, 1.0f));//黄
            }else if(i == 4){
                setDiffuseColor( ci::ColorA(0.0f, 1.0f, 1.0f, 1.0f));//黄緑？
            }else if(i == 5){
                setDiffuseColor( ci::ColorA(1.0f, 0.0f, 1.0f, 1.0f));//紫
            }else if(i == 6){
                setDiffuseColor( ci::ColorA(0.53f, 0.0f, 0.85f, 1.0f));
            }else if(i == 7){
                setDiffuseColor( ci::ColorA(0.5f, 0.5f, 0.5f, 1.0f));
            }else if(i == 8){
                setDiffuseColor( ci::ColorA(1.0f, 1.0f, 1.0f, 1.0f));
            }else{
                setDiffuseColor( ci::ColorA(0.65, 0.83, 0.58));
            }

            
            gl::pushMatrices();
            gl::drawString(messageList[i],Vec2f(995, 100 + (70 * i)), ci::ColorA(0.83, 0.62, 0.53), mFont);
            gl::translate(Vec2f(980, 100 + (70 * i)));
            drawCircle3();
            gl::popMatrices();
        }
        setDiffuseColor( ci::ColorA(0.65, 0.83, 0.58));
    }
    
    //説明用の円
    void drawCircle3(){
        //説明用の円
        gl::pushMatrices();
        gl::drawSolidCircle(Vec2d( 0, 0), 10);
        gl::popMatrices();
    }
    
    void drawAccessNumber(){
        
        int h = sumHands();
        int j1 = sumJes1();
        int j2 = sumJes2();
        int j3 = sumJes3();
        int j4 = sumJes4();
        int cmn = countMessageNumber();
        printf("countMessageNumberの値：%d\n", cmn);
        gl::drawString("トータルアクセス数：" + to_string(sumOfFrag()), Vec2d(400,820));
        gl::drawString("一番多いメッセージナンバー：" + to_string(cmn), Vec2d(600,820));
        gl::drawString("手の数：" + to_string(h), Vec2d(850,820));
        gl::drawString("サークル数：" + to_string(j1), Vec2d(1000,820));
        gl::drawString("スワイプ数：" + to_string(j2), Vec2d(1100,820));
        gl::drawString("タップ数：" + to_string(j3+j4), Vec2d(1200,820));
    }
    
    void drawHelp(){
        ////円の中心からメッセージリストまで
        //円の中心からメッセージリストの一番上の線まで引く線
        gl::pushMatrices();
        gl::drawLine(Vec2f( 360, WindowHeight/2 ), Vec2f(920, 100));
        gl::popMatrices();
        //メッセージリストの縦に引く線
        gl::pushMatrices();
        gl::drawLine(Vec2f(950, 100), Vec2f(950, 660));
        gl::popMatrices();
        //メッセージリストへ横に引く線
        gl::pushMatrices();
        gl::drawLine(Vec2f(920, 100), Vec2f(980, 100));
        for (int i = 1; i < 9; i++) {
            gl::drawLine(Vec2f(950, 100 + (70 * i)), Vec2f(980, 100 + (70 * i)));
        }
        gl::popMatrices();
        
        ////円の周回運動から
        //円から曲点まで
        gl::pushMatrices();
        gl::drawLine(Vec2f( rad * 10 + 360, WindowHeight/2 ), Vec2f(rad * 10 + 360 + 50, 500));
        gl::popMatrices();
        //曲点から端まで
        gl::pushMatrices();
        gl::drawLine(Vec2f(rad * 10 + 360 + 50, 500), Vec2f(rad * 10 + 360 + 170, 500));
        gl::drawSolidCircle(Vec2f(rad * 10 + 360 + 170, 500), 5);
        gl::drawString("サークル数+スワイプ数", Vec2f(rad * 10 + 360 + 50, 480));
        gl::popMatrices();
        
        ////円の周回運動から半径のHelp
        //円から曲点まで
        gl::pushMatrices();
        gl::drawLine(Vec2f(360, -rad * 8 * -1 + WindowHeight/2 ), Vec2f(360 + rad * 8, 560));
        gl::popMatrices();
        //曲点から端まで
        gl::pushMatrices();
        gl::drawLine(Vec2f(360 + rad * 8, 560), Vec2f(360 + 120 + rad * 8, 560));
        gl::drawSolidCircle(Vec2f(360 + 120 + rad * 8, 560), 5);
        gl::drawString("タップ数", Vec2f(360 + rad * 8, 540));
        gl::popMatrices();
    
    }
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
    //バックグラウンド
    gl::Texture backgroundImage;
    
    //フォント
    Font mFont;
    Color mFontColor;
    
    //パラメーター表示する時に使う
    params::InterfaceGl mParams;

    
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
    int handSpeed;//サークルジェスチャーの回数
    int circleSpeed;//円運動の速さ
    int handRadius;//スクリーンタップジェスチャーの回数
    int rad;
    
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
    if (msgInfo.count[6] == -1) {
        //msgInfo構造体count[6]の値（メッセージの番号）が-1のときはなにもしない
    }else if(((msgInfo.count[2] > 1 )||(msgInfo.count[3] > 1)||(msgInfo.count[4] > 1)||(msgInfo.count[5] > 1))&&(msgInfo.count[6] != -1)){
        allMessage[accountNumber] = msgInfo;//それ以外のときはallMessageに記録
        countMessageNumber();
        leapLog(accountNumber);
        debag(accountNumber);//記録したものをデバッグする
    }
    
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
    for (int i = 1; i < 7; i++) {
       msg.count[i] = atoi(strtok(NULL, ","));
    }
    return msg;
}

//アクセス数を返す関数
int sumOfFrag(){
    
    int sum =0;
    for (int i = 0; i < 6; i++) {
        sum = sum + allMessage[i].flag;
    }
    return sum;
}

//送られてきた手の数を集計して、合計した値を返す関数
int sumHands(){
    int hands = 0;
    for (int i = 0; i < 6 ; i++) {
        if (allMessage[i].flag == 1) {
            hands = hands + allMessage[i].count[1];
        }
    }
    return hands;
}

//送られてきたジェスチャーの数を集計して、合計した値を返す関数
int sumJes1(){
    //circleを集計する
    int jes1 = 0;
    for (int i = 0; i < 6 ; i++) {
        if (allMessage[i].flag == 1) {
            //ジェスチャーが選択されていないときは0を入れる
            if (allMessage[i].count[2] == -1) {
                allMessage[i].count[2] = 0;
            }
            jes1 = jes1 + allMessage[i].count[2];
        }
    }
    return jes1;
}

//送られてきたジェスチャーの数を集計して、合計した値を返す関数
int sumJes2(){
    //スワイプを集計する
    int jes2 = 0;
    for (int i = 0; i < 6 ; i++) {
        if (allMessage[i].flag == 1) {
            //ジェスチャーが選択されていないときは0を入れる
            if (allMessage[i].count[3] == -1) {
                allMessage[i].count[3] = 0;
            }
            jes2 = jes2 + allMessage[i].count[3];
        }
    }
    return jes2;
}
int sumJes3(){
    //スクリーンタップを集計する
    int jes3 = 0;
    for (int i = 0; i < 6 ; i++) {
        if (allMessage[i].flag == 1) {
            //ジェスチャーが選択されていないときは0を入れる
            if (allMessage[i].count[4] == -1) {
                allMessage[i].count[4] = 0;
            }
            jes3 = jes3 + allMessage[i].count[4];
        }
    }
    return jes3;
}
int sumJes4(){
    //キータップジェスチャーを集計する
    int jes4 = 0;
    for (int i = 0; i < 6 ; i++) {
        if (allMessage[i].flag == 1) {
            //ジェスチャーが選択されていないときは0を入れる
            if (allMessage[i].count[5] == -1) {
                allMessage[i].count[5] = 0;
            }
            jes4 = jes4 + allMessage[i].count[5];
        }
    }
    return jes4;
}


//送られてきたメッセージの中でどれが多いかを算出し、返す関数
int countMessageNumber(){
    int sumMessageNumber[9] = {0,0,0,0,0,0,0,0,0};
    int resultNumber = 0;//返す値
    
    //記録しているぶんのmessageの値を参照して、その要素に加えていく
    
    for (int i = 0; i < 6; i++) {
        if (allMessage[i].flag == 1) {
            if (allMessage[i].count[6]== 0) {
                mcount++;
                sumMessageNumber[0] = mcount;
            }
            else if (allMessage[i].count[6]== 1) {
                mcount2++;
                sumMessageNumber[1] = mcount2;
            }
            else if (allMessage[i].count[6]== 2) {
                mcount3++;
                sumMessageNumber[2] = mcount3;
            }
            else if (allMessage[i].count[6]== 3 ) {
                mcount4++;
                sumMessageNumber[3] = mcount4;
            }
            else if (allMessage[i].count[6]== 4) {
                mcount5++;
                sumMessageNumber[4] = mcount5;
            }
            else if (allMessage[i].count[6]== 5 ) {
                mcount6++;
                sumMessageNumber[5] = mcount6;
            }
            else if (allMessage[i].count[6]== 6) {
                mcount7++;
                sumMessageNumber[6] = mcount7;
            }
            else if (allMessage[i].count[6]== 7) {
                mcount8++;
                sumMessageNumber[7] = mcount8;
            }
            else if (allMessage[i].count[6]== 8 ) {
                mcount9++;
                sumMessageNumber[8] = mcount9;
            }else{
                
            }
        }
    }
    //初期値に戻す
    
    //要素数が最大の番地を求める
    for (int i = 0; i < 9; i++) {
        if (resultNumber < sumMessageNumber[i]) {
            resultNumber = i;
        }
    }
    mcount = 0;
    mcount2 = 0;
    mcount3 = 0;
    mcount4 = 0;
    mcount5 = 0;
    mcount6 = 0;
    mcount7 = 0;
    mcount8 = 0;
    mcount9 = 0;
    
    printf("最大の番号：%d\n", resultNumber);
    return resultNumber;
}

void leapLog(int number){
    FILE *outputfile;         // 出力ストリーム
    
    outputfile = fopen("../data.txt", "a");  // ファイルを書き込み用にオープン(開く)
    if (outputfile == NULL) {          // オープンに失敗した場合
        printf("cannot open\n");         // エラーメッセージを出して
        exit(1);                         // 異常終了
    }
    fprintf(outputfile, "Time:%s, circle:%d, swipe:%d, stap:%d, ktap:%d, message:%d .\n",
                        ctime(&allMessage[number].time), allMessage[number].count[2],allMessage[number].count[3], allMessage[number].count[4], allMessage[number].count[5],allMessage[number].count[6]); // ファイルに書く}

    
    fclose(outputfile);          // ファイルをクローズ(閉じる)
}

//構造体の中身を確認するための関数
void debag(int number){
    //送られてきたアカウント名のデータをプリントする
//        printf("時間：%ld\n", allMessage[number].time);
//        printf("フラグ：%d\n", allMessage[number].flag );
//        printf("count[0]の値：%d\n", allMessage[number].count[0]);
//        printf("count[1]の値：%d\n", allMessage[number].count[1]);
//        printf("count[2]の値：%d\n", allMessage[number].count[2]);
//        printf("count[3]の値：%d\n", allMessage[number].count[3]);
//        printf("count[4]の値：%d\n", allMessage[number].count[4]);
//    for (int i = 0; i < 7; i++) {
//        printf("i番さんのサークルの値：%d\n", allMessage[i].count[2]);
//    }
//    for (int i = 0; i < 7; i++) {
//        printf("i番さんのタップの値：%d\n", allMessage[i].count[3]);
//    }
//    printf("どのメッセージが多いか：%d\n", countMessageNumber());
//    printf("フラグの合計値：%d\n", sumOfFrag() );
//    printf("手の数合計値：%d\n", sumHands());
//    printf("サークルの合計値：%d\n", sumJes1());
//    printf("スクリーンタップの合計値：%d\n", sumJes2());
//    
//    for (int i = 0; i < 6 ; i++) {
//        printf("%d番のメッセージのフラグ：%d\n", i, allMessage[i].count[4]);
//    }

}
