#include <OpenGL2Writer.hpp>

#include <QMPlay2_OSD.hpp>
#include <Functions.hpp>

#ifndef USE_NEW_OPENGL_API
	#include <QGLShaderProgram>
#else
	#include <QOpenGLShaderProgram>
	#include <QOpenGLContext>
#endif
#include <QPainter>

#if 0
	typedef HRESULT (WINAPI *DwmIsCompositionEnabledProc)(BOOL *pfEnabled);
#endif

#if !defined OPENGL_ES2 && !defined Q_OS_MAC
	#include <GL/glext.h>
#endif

#ifdef OPENGL_ES2
	static const char precisionStr[] = "precision lowp float;";
#else
	static const char precisionStr[] = "";
#endif

static const char vShaderYCbCrSrc[] =
	"%1"
	"attribute vec4 vPosition;"
	"attribute vec2 aTexCoord;"
	"varying vec2 vTexCoord;"
	"uniform vec2 scale;"
	"void main() {"
		"vTexCoord = aTexCoord;"
		"gl_Position = vPosition * vec4(scale.xy, 1, 1);"
	"}";
static const char fShaderYCbCrSrc[] =
	"%1"
	"varying vec2 vTexCoord;"
	"uniform vec4 videoEq;"
	"uniform sampler2D Ytex, Utex, Vtex;"
	"void main() {"
		"float brightness = videoEq[0];"
		"float contrast = videoEq[1];"
		"float saturation = videoEq[2];"
		"vec3 YCbCr = vec3("
			"texture2D(Ytex, vTexCoord)[0] - 0.0625,"
			"texture2D(Utex, vTexCoord)[0] - 0.5,"
			"texture2D(Vtex, vTexCoord)[0] - 0.5"
		");"
		"%2"
		"YCbCr.yz *= saturation;"
		"vec3 rgb = mat3(1.1643, 1.1643, 1.1643, 0.0, -0.39173, 2.017, 1.5958, -0.8129, 0.0) * YCbCr * contrast + brightness;"
		"gl_FragColor = vec4(rgb, 1.0);"
	"}";
static const char fShaderYCbCrHueSrc[] =
	"float hueAdj = videoEq[3];"
	"if (hueAdj != 0.0) {"
		"float hue = atan(YCbCr[2], YCbCr[1]) + hueAdj;"
		"float chroma = sqrt(YCbCr[1] * YCbCr[1] + YCbCr[2] * YCbCr[2]);"
		"YCbCr[1] = chroma * cos(hue);"
		"YCbCr[2] = chroma * sin(hue);"
	"}";

static const char vShaderOSDSrc[] =
	"%1"
	"attribute vec4 vPosition;"
	"attribute vec2 aTexCoord;"
	"varying vec2 vTexCoord;"
	"void main() {"
		"vTexCoord = aTexCoord;"
		"gl_Position = vPosition;"
	"}";
static const char fShaderOSDSrc[] =
	"%1"
	"varying vec2 vTexCoord;"
	"uniform sampler2D tex;"
	"void main() {"
		"gl_FragColor = texture2D(tex, vTexCoord);"
	"}";

static const float verticesYCbCr[ 4 ][ 8 ] = {
	/* Normal */
	{
		-1.0f, -1.0f, //0. Left-bottom
		+1.0f, -1.0f, //1. Right-bottom
		-1.0f, +1.0f, //2. Left-top
		+1.0f, +1.0f, //3. Right-top
	},
	/* Horizontal flip */
	{
		+1.0f, -1.0f, //1. Right-bottom
		-1.0f, -1.0f, //0. Left-bottom
		+1.0f, +1.0f, //3. Right-top
		-1.0f, +1.0f, //2. Left-top
	},
	/* Vertical flip */
	{
		-1.0f, +1.0f, //2. Left-top
		+1.0f, +1.0f, //3. Right-top
		-1.0f, -1.0f, //0. Left-bottom
		+1.0f, -1.0f, //1. Right-bottom
	},
	/* Rotated */
	{
		+1.0f, +1.0f, //3. Right-top
		-1.0f, +1.0f, //2. Left-top
		+1.0f, -1.0f, //1. Right-bottom
		-1.0f, -1.0f, //0. Left-bottom
	}
};
static const float texCoordOSD[ 8 ] = {
	0.0f, 1.0f,
	1.0f, 1.0f,
	0.0f, 0.0f,
	1.0f, 0.0f,
};

/**/

#ifndef USE_NEW_OPENGL_API
Drawable::Drawable( OpenGL2Writer &writer, const QGLFormat &fmt ) :
	QGLWidget( fmt ),
#else
Drawable::Drawable( OpenGL2Writer &writer ) :
#endif
	isOK( true ), paused( false ),
	videoFrame( NULL ),
	shaderProgramYCbCr( NULL ), shaderProgramOSD( NULL ),
#ifndef OPENGL_ES2
	glActiveTexture( NULL ),
#endif
	writer( writer ),
	hasImage( false )
{
	grabGesture( Qt::PinchGesture );
	setMouseTracking( true );

#if 0
	/*
	 * Windows Vista and newer can use compositing (DWM) which is enabled by default.
	 * It prevents tearing, so if the DWM is enabled, use single buffer. Otherwise on
	 * AMD and nVidia GPU on fullscreen the toolbar and context menu are invisible.
	 *
	 * VSync doesn't work when single buffer is chosen, so DWM must prevents tearing.
	 * This is not 3D game, so it should be enough.
	 *
	 * Since Windows 8, the DWM is probably always enabled.
	*/
	if ( QSysInfo::windowsVersion() >= QSysInfo::WV_6_0 )
	{
		DwmIsCompositionEnabledProc DwmIsCompositionEnabled = ( DwmIsCompositionEnabledProc )GetProcAddress( GetModuleHandleA( "dwmapi.dll" ), "DwmIsCompositionEnabled" );
		BOOL compositionEnabled = false;
		if ( DwmIsCompositionEnabled && DwmIsCompositionEnabled( &compositionEnabled ) == S_OK && compositionEnabled )
		{
			QGLFormat fmt;
			fmt.setDoubleBuffer( false );
			setFormat( fmt );
		}
	}
#endif

	/* Initialize texCoord array */
	texCoordYCbCr[ 0 ] = texCoordYCbCr[ 4 ] = texCoordYCbCr[ 5 ] = texCoordYCbCr[ 7 ] = 0.0f;
	texCoordYCbCr[ 1 ] = texCoordYCbCr[ 3 ] = 1.0f;
}

#ifndef USE_NEW_OPENGL_API
bool Drawable::init()
{
	makeCurrent();
	if ( ( isOK = isValid() ) )
		glInit();
	doneCurrent();
	return isOK;
}
#endif
void Drawable::clr()
{
	hasImage = false;
	osdImg = QImage();
	osd_checksums.clear();
}

void Drawable::resizeEvent( QResizeEvent *e )
{
	Functions::getImageSize( writer.aspect_ratio, writer.zoom, width(), height(), W, H, &X, &Y );
	doReset = true;
	if ( e )
		QGLWidget::resizeEvent( e );
	else if ( paused )
	{
#ifndef USE_NEW_OPENGL_API
		updateGL();
#else
		repaint();
#endif
	}
}

void Drawable::initializeGL()
{
#ifndef OPENGL_ES2
	bool supportsShaders = false, canCreateNonPowerOfTwoTextures = false;
	const char *glExtensions = ( const char * )glGetString( GL_EXTENSIONS );
	if ( glExtensions )
	{
		supportsShaders = !!strstr( glExtensions, "GL_ARB_vertex_shader" ) && !!strstr( glExtensions, "GL_ARB_fragment_shader" ) && !!strstr( glExtensions, "GL_ARB_shader_objects" );
		canCreateNonPowerOfTwoTextures = !!strstr( glExtensions, "GL_ARB_texture_non_power_of_two" );
	}
	glActiveTexture = ( GLActiveTexture )context()->getProcAddress( "glActiveTexture" );

	if
	(
		!canCreateNonPowerOfTwoTextures ||
		!supportsShaders                ||
		!glActiveTexture
	)
	{
		/*
		 * If shader programs are already exists, new context was created without shaders support...
		 * So the GPU/driver supports this feature and this is workaround for this strange behaviour.
		*/
		if ( shaderProgramYCbCr && shaderProgramOSD )
			fprintf( stderr, "Shaders are already created and now they are not supported... Initialization is ignored.\n" );
		else
		{
			fprintf
			(
				stderr,
				"GL_ARB_texture_non_power_of_two : %s\n"
				"Vertex & fragment shader: %s\n"
				"glActiveTexture: %s\n",
				canCreateNonPowerOfTwoTextures ? "yes" : "no",
				supportsShaders ? "yes" : "no",
				glActiveTexture ? "yes" : "no"
			);
			QMPlay2Core.logError( "OpenGL 2 :: " + tr( "Sterownik musi obsługiwać multiteksturowanie, shadery oraz tekstury o dowolnym rozmiarze" ), true, true );
			isOK = false;
		}
		return;
	}
#endif

	delete shaderProgramYCbCr;
	delete shaderProgramOSD;
	shaderProgramYCbCr = new QGLShaderProgram( this );
	shaderProgramOSD = new QGLShaderProgram( this );

	/* YCbCr shader */
	const char *glslVersionStr = ( const char * )glGetString( GL_SHADING_LANGUAGE_VERSION );
	bool useHue = false;
	if ( glslVersionStr )
	{
		/* Don't use hue if GLSL is lower than 1.3, because it can be slow on old hardware and may increase CPU usage! */
		const float glslVersion = QString( glslVersionStr ).left( 4 ).toFloat();
#ifndef OPENGL_ES2
		useHue = glslVersion >= 1.3f;
#else
		useHue = glslVersion >= 3.0f;
#endif
	}
	shaderProgramYCbCr->addShaderFromSourceCode( QGLShader::Vertex, QString( vShaderYCbCrSrc ).arg( precisionStr ) );
	shaderProgramYCbCr->addShaderFromSourceCode( QGLShader::Fragment, QString( fShaderYCbCrSrc ).arg( precisionStr ).arg( useHue ? fShaderYCbCrHueSrc : "" ) );
	if ( shaderProgramYCbCr->bind() )
	{
		texCoordYCbCrLoc = shaderProgramYCbCr->attributeLocation( "aTexCoord" );
		positionYCbCrLoc = shaderProgramYCbCr->attributeLocation( "vPosition" );

		shaderProgramYCbCr->setUniformValue( "Ytex", 0 );
		shaderProgramYCbCr->setUniformValue( "Utex", 1 );
		shaderProgramYCbCr->setUniformValue( "Vtex", 2 );

		shaderProgramYCbCr->release();
	}
	else
	{
		QMPlay2Core.logError( tr( "Błąd podczas kompilacji/linkowania shaderów" ) );
		isOK = false;
		return;
	}

	/* OSD shader */
	shaderProgramOSD->addShaderFromSourceCode( QGLShader::Vertex, QString( vShaderOSDSrc ).arg( precisionStr ) );
	shaderProgramOSD->addShaderFromSourceCode( QGLShader::Fragment, QString( fShaderOSDSrc ).arg( precisionStr ) );
	if ( shaderProgramOSD->bind() )
	{
		texCoordOSDLoc = shaderProgramOSD->attributeLocation( "aTexCoord" );
		positionOSDLoc = shaderProgramOSD->attributeLocation( "vPosition" );

		shaderProgramOSD->setUniformValue( "tex", 3 );

		shaderProgramOSD->release();
	}
	else
	{
		QMPlay2Core.logError( tr( "Błąd podczas kompilacji/linkowania shaderów" ) );
		isOK = false;
		return;
	}

	/* Set OpenGL parameters */
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glClearColor( 0.0, 0.0, 0.0, 1.0 );
	glClear( GL_COLOR_BUFFER_BIT );
	glDisable( GL_STENCIL_TEST );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_DITHER );

	/* Prepare textures */
	for ( int i = 1 ; i <= 4 ; ++i )
	{
		glBindTexture( GL_TEXTURE_2D, i );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, i == 1 ? GL_NEAREST : GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, i == 1 ? GL_NEAREST : GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}

#ifdef VSYNC_SETTINGS
	/* Ensures that the VSync will be set on paintGL() */
	lastVSyncState = !writer.vSync;
#endif

	doClear = 0;
}
void Drawable::paintGL()
{
#ifdef VSYNC_SETTINGS
	if ( lastVSyncState != writer.vSync )
	{
		typedef int (APIENTRY *SwapInterval)(int); //BOOL is just normal int in Windows, APIENTRY declares nothing on non-Windows platforms
		SwapInterval swapInterval = NULL;
#ifdef Q_OS_WIN
		swapInterval = ( SwapInterval )context()->getProcAddress( "wglSwapIntervalEXT" );
#else
		swapInterval = ( SwapInterval )context()->getProcAddress( "glXSwapIntervalMESA" );
		if ( !swapInterval )
			swapInterval = ( SwapInterval )context()->getProcAddress( "glXSwapIntervalSGI" );
#endif
		if ( swapInterval )
			swapInterval( writer.vSync );
		else
			QMPlay2Core.logError( tr( "Zarządzanie VSync jest nieobsługiwane" ), true, true );
		lastVSyncState = writer.vSync;
	}
#endif

	if ( doReset )
		doClear = 3; //3 buffers must be cleared when triple-buffer is used
	if ( doClear > 0 )
	{
		glClear( GL_COLOR_BUFFER_BIT );
		--doClear;
	}

	if ( !videoFrame && !hasImage )
		return;

	if ( videoFrame )
	{
		hasImage = true;

		if ( doReset )
		{
			/* Prepare textures */
			glBindTexture( GL_TEXTURE_2D, 2 );
			glTexImage2D( GL_TEXTURE_2D, 0, GL_LUMINANCE, videoFrame->linesize[ 0 ], writer.outH, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL );

			glBindTexture( GL_TEXTURE_2D, 3 );
			glTexImage2D( GL_TEXTURE_2D, 0, GL_LUMINANCE, videoFrame->linesize[ 1 ], writer.outH >> 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL );

			glBindTexture( GL_TEXTURE_2D, 4 );
			glTexImage2D( GL_TEXTURE_2D, 0, GL_LUMINANCE, videoFrame->linesize[ 2 ], writer.outH >> 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL );

			/* Prepare texture coordinates */
			texCoordYCbCr[ 2 ] = texCoordYCbCr[ 6 ] = (videoFrame->linesize[ 0 ] == writer.outW) ? 1.0f : (writer.outW / (videoFrame->linesize[ 0 ] + 1.0f));
		}

		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_2D, 2 );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, videoFrame->linesize[ 0 ], writer.outH, GL_LUMINANCE, GL_UNSIGNED_BYTE, videoFrame->data[ 0 ] );

		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_2D, 3 );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, videoFrame->linesize[ 1 ], writer.outH >> 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, videoFrame->data[ 1 ] );

		glActiveTexture( GL_TEXTURE2 );
		glBindTexture( GL_TEXTURE_2D, 4 );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, videoFrame->linesize[ 2 ], writer.outH >> 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, videoFrame->data[ 2 ] );
	}

	shaderProgramYCbCr->setAttributeArray( positionYCbCrLoc, verticesYCbCr[ writer.flip ], 2 );
	shaderProgramYCbCr->setAttributeArray( texCoordYCbCrLoc, texCoordYCbCr, 2 );
	shaderProgramYCbCr->enableAttributeArray( positionYCbCrLoc );
	shaderProgramYCbCr->enableAttributeArray( texCoordYCbCrLoc );

	shaderProgramYCbCr->bind();
	if ( doReset )
	{
		shaderProgramYCbCr->setUniformValue( "scale", W / ( float )width(), H / ( float )height() );
		shaderProgramYCbCr->setUniformValue( "videoEq", Brightness, Contrast, Saturation, Hue );
		doReset = false;
	}
	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
	shaderProgramYCbCr->release();

	shaderProgramYCbCr->disableAttributeArray( texCoordYCbCrLoc );
	shaderProgramYCbCr->disableAttributeArray( positionYCbCrLoc );

	glActiveTexture( GL_TEXTURE3 );

	/* OSD */
	osd_mutex.lock();
	if ( !osd_list.isEmpty() )
	{
		glBindTexture( GL_TEXTURE_2D, 1 );

		QRect bounds;
		const qreal scaleW = ( qreal )W / writer.outW, scaleH = ( qreal )H / writer.outH;
		bool mustRepaint = Functions::mustRepaintOSD( osd_list, osd_checksums, &scaleW, &scaleH, &bounds );
		if ( !mustRepaint )
			mustRepaint = osdImg.size() != bounds.size();
		if ( mustRepaint )
		{
			if ( osdImg.size() != bounds.size() )
				osdImg = QImage( bounds.size(), QImage::Format_ARGB32 );
			osdImg.fill( 0 );
			QPainter p( &osdImg );
			p.translate( -bounds.topLeft() );
			Functions::paintOSD( false, osd_list, scaleW, scaleH, p, &osd_checksums );
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, bounds.width(), bounds.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, osdImg.bits() );
		}

		const float left   = ( bounds.left() + X ) * 2.0f / width();
		const float right  = ( bounds.right() + X + 1 ) * 2.0f / width();
		const float top    = ( bounds.top() + Y ) * 2.0f / height();
		const float bottom = ( bounds.bottom() + Y + 1 ) * 2.0f / height();
		const float verticesOSD[ 8 ] = {
			left  - 1.0f, -bottom + 1.0f,
			right - 1.0f, -bottom + 1.0f,
			left  - 1.0f, -top    + 1.0f,
			right - 1.0f, -top    + 1.0f,
		};

		shaderProgramOSD->setAttributeArray( positionOSDLoc, verticesOSD, 2 );
		shaderProgramOSD->setAttributeArray( texCoordOSDLoc, texCoordOSD, 2 );
		shaderProgramOSD->enableAttributeArray( positionOSDLoc );
		shaderProgramOSD->enableAttributeArray( texCoordOSDLoc );

		glEnable( GL_BLEND );
		shaderProgramOSD->bind();
		glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
		shaderProgramOSD->release();
		glDisable( GL_BLEND );

		shaderProgramOSD->disableAttributeArray( texCoordOSDLoc );
		shaderProgramOSD->disableAttributeArray( positionOSDLoc );
	}
	osd_mutex.unlock();

	glBindTexture( GL_TEXTURE_2D, 0 );
}
#ifndef USE_NEW_OPENGL_API
void Drawable::resizeGL( int w, int h )
{
	glViewport( 0, 0, w, h );
}
#endif

bool Drawable::event( QEvent *e )
{
	/*
	 * QGLWidget blocks this event forever (tested on Windows 8.1, Qt 4.8.7)
	 * This is workaround: pass gesture event to the parent.
	*/
	if ( e->type() == QEvent::Gesture )
		return qApp->notify( parent(), e );
	return QGLWidget::event( e );
}

/**/

OpenGL2Writer::OpenGL2Writer( Module &module ) :
	outW( -1 ), outH( -1 ), W( -1 ), flip( 0 ),
	aspect_ratio( 0.0 ), zoom( 0.0 ),
#ifdef VSYNC_SETTINGS
	vSync( true ),
#endif
	drawable( NULL )
{
	addParam( "W" );
	addParam( "H" );
	addParam( "AspectRatio" );
	addParam( "Zoom" );
	addParam( "Flip" );
	addParam( "Saturation" );
	addParam( "Brightness" );
	addParam( "Contrast" );
	addParam( "Hue" );

	SetModule( module );
}
OpenGL2Writer::~OpenGL2Writer()
{
	delete drawable;
}

bool OpenGL2Writer::set()
{
#ifdef VSYNC_SETTINGS
	vSync = sets().getBool( "VSync" );
#endif
	return sets().getBool( "Enabled" );
}

bool OpenGL2Writer::readyWrite() const
{
	return drawable->isOK;
}

bool OpenGL2Writer::processParams( bool * )
{
	bool doResizeEvent = false;

	const double _aspect_ratio = getParam( "AspectRatio" ).toDouble();
	const double _zoom = getParam( "Zoom" ).toDouble();
	const int _flip = getParam( "Flip" ).toInt();
	const float Contrast = ( getParam( "Contrast" ).toInt() + 100 ) / 100.0f;
	const float Saturation = ( getParam( "Saturation" ).toInt() + 100 ) / 100.0f;
	const float Brightness = getParam( "Brightness" ).toInt() / 100.0f;
	const float Hue = getParam( "Hue" ).toInt() / -31.831f;
	if ( _aspect_ratio != aspect_ratio || _zoom != zoom || _flip != flip || drawable->Contrast != Contrast || drawable->Brightness != Brightness || drawable->Saturation != Saturation || drawable->Hue != Hue )
	{
		zoom = _zoom;
		aspect_ratio = _aspect_ratio;
		flip = _flip;
		drawable->Contrast = Contrast;
		drawable->Brightness = Brightness;
		drawable->Saturation = Saturation;
		drawable->Hue = Hue;
		doResizeEvent = drawable->isVisible();
	}

	const int _outW = getParam( "W" ).toInt();
	const int _outH = getParam( "H" ).toInt();
	if ( _outW > 0 && _outH > 0 && ( _outW != outW || _outH != outH ) )
	{
		outW = _outW;
		outH = _outH;
		W = ( outW % 8 ) ? outW-1 : outW;

		drawable->clr();
		emit QMPlay2Core.dockVideo( drawable );
	}

	if ( doResizeEvent )
		drawable->resizeEvent( NULL );
	else
		drawable->doReset = true;

	return readyWrite();
}

qint64 OpenGL2Writer::write( const QByteArray &arr )
{
	drawable->paused = false;
	drawable->videoFrame = VideoFrame::fromData( arr );
#ifndef USE_NEW_OPENGL_API
	drawable->updateGL();
#else
	drawable->repaint();
#endif
	drawable->videoFrame = NULL;
	VideoFrame::unref( arr );
	return arr.size();
}
void OpenGL2Writer::writeOSD( const QList< const QMPlay2_OSD * > &osds )
{
	drawable->osd_mutex.lock();
	drawable->osd_list = osds;
	drawable->osd_mutex.unlock();
}

void OpenGL2Writer::pause()
{
	drawable->paused = true;
}

QString OpenGL2Writer::name() const
{
#ifdef OPENGL_ES2
	return "OpenGL|ES 2";
#else
	return OpenGL2WriterName;
#endif
}

bool OpenGL2Writer::open()
{
#ifndef USE_NEW_OPENGL_API
	QGLFormat fmt;
	fmt.setDepth( false );
	fmt.setStencil( false );
	drawable = new Drawable( *this, fmt );
	return drawable->init();
#else
	drawable = new Drawable( *this );
	return true;
#endif
}