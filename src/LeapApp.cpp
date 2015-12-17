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
        mFont = Font( "YuGothic", 20 );
        
        
        // カメラ(視点)の設定
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
 
        // アルファブレンディングを有効にする
//        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//        gl::enable(GL_BLEND);
        
        //backgroundImageの読み込み
        //backgroundImage = gl::Texture(loadImage(loadResource("../resources/image.jpg")));
        backgroundImage = gl::Texture(loadImage(loadResource(BACKGROUND)));
        
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
    }
    
    //描写処理
    /*
    void *draw(void *p){
        if(!buffer){
            gl::clear();
            gl::pushMatrices();// カメラ位置を設定する
            gl::setMatrices( mMayaCam.getCamera() );
            drawMarionette();//マリオネット描写
            drawListArea();//メッセージリストの表示
            gl::popMatrices();
            // パラメーター設定UIを描画する
        }
        return NULL;
    }*/
    //描写処理
    void draw(){
        gl::clear();
        gl::enableDepthRead();
        gl::enableDepthWrite();

        gl::pushMatrices();
            drawListArea();//メッセージリストの表示
            drawCircle();//サークルで表示
            //drawPainting();//指の軌跡を描く
            //drawAudioAnalyze();//音声解析の描写
            //drawSinGraph();//sinグラフを描く
            drawBarGraph();
            //drawBox();
            drawAccessNumber();
        gl::popMatrices();
        
        for(int i = 0; i < sumOfFrag(); i++){
            gl::pushMatrices();
            translate(Vec2d(i*200,0));
            drawMarionette();//マリオネット描写
            gl::popMatrices();
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
    //メッセージリスト
    void drawListArea(){
        //stringstream mm;
//        gl::pushMatrices();
//        auto tbox0 = TextBox().alignment( TextBox::LEFT ).font( mFont ).text ( mm.str() ).color(Color( 1.0f, 1.0f, 1.0f )).backgroundColor( ColorA( 0, 1.0f, 0, 0 ) );
//        auto mTextTexture = gl::Texture( tbox0.render() );
//        gl::draw( mTextTexture );
//  
//        gl::popMatrices();
//        if(l < 0){
//            gl::drawString(message, Vec2d(0,0));
//        }
    }
    
    //サークル（手の数によって大きくなる球体の描写）
    void drawCircle(){
        //sine, cosineを使った曲線的な拡大縮小///////////////////////////
        //この場合-A*sin(w*radians(t) - p)の計算結果は100.0~-100.0なので、
        //100を足すことによって、0~200にしている。
        int handRadius = sumHands();
        //y = A*sin(w*(t * PI / 180.0) - p) + 100;
        printf("drawCircle:%d\n",handRadius * 10);
        gl::pushMatrices();
        gl::drawSphere(Vec3d( 360, 675, 0 ), handRadius*10);//指の位置
        gl::drawString(toString(handRadius), Vec2d( 360, 100));
        gl::popMatrices();

    }
    void drawAccessNumber(){
        gl::drawString("トータルアクセス数", Vec2d(1200,700));
        gl::drawString(to_string(sumOfFrag()), Vec2d(1200,800));
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
    float defFaceTransX = 200.0;//顔のx座標の位置
    float defFaceTransY = 675-110.0;//顔のy座標の位置
    float defFaceTransZ = 0.0;//顔のz座標の位置
    
    float defBodyTransX = 200.0;//体のx座標の位置
    float defBodyTransY = 675.0;//体のy座標の位置
    float defBodyTransZ = 0.0;//体のz座標の位置
    
    float defLeftArmTransX=200.0+75.0;
    float defRightArmTransX=200.0-75.0;
    float defArmTransY=675+20.0;
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
