/** 
 * @file llui.h
 * @brief GL function declarations and other general static UI services.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

// All immediate-mode gl drawing should happen here.

#ifndef LL_LLUI_H
#define LL_LLUI_H

#include "llpointer.h"		// LLPointer<>
#include "llrect.h"
#include "llcontrol.h"
#include "llcoord.h"
#include "llglslshader.h"
//#include "llhtmlhelp.h"
#include "llgl.h"			// *TODO: break this dependency
#include <stack>
#include "lltexture.h"
#include <boost/signals2.hpp>

// LLUIFactory
#include "llsd.h"

class LLColor4; 
class LLHtmlHelp;
class LLVector3;
class LLVector2;
class LLUIImage;
class LLUUID;
class LLWindow;
class LLView;

// UI colors
extern const LLColor4 UI_VERTEX_COLOR;
void make_ui_sound(const char* name);

BOOL ui_point_in_rect(S32 x, S32 y, S32 left, S32 top, S32 right, S32 bottom);
void gl_state_for_2d(S32 width, S32 height);

void gl_line_2d(S32 x1, S32 y1, S32 x2, S32 y2);
void gl_line_2d(S32 x1, S32 y1, S32 x2, S32 y2, const LLColor4 &color );
void gl_triangle_2d(S32 x1, S32 y1, S32 x2, S32 y2, S32 x3, S32 y3, const LLColor4& color, BOOL filled);
void gl_rect_2d_simple( S32 width, S32 height );

void gl_draw_x(const LLRect& rect, const LLColor4& color);

void gl_rect_2d(S32 left, S32 top, S32 right, S32 bottom, BOOL filled = TRUE );
void gl_rect_2d(S32 left, S32 top, S32 right, S32 bottom, const LLColor4 &color, BOOL filled = TRUE );
void gl_rect_2d_offset_local( S32 left, S32 top, S32 right, S32 bottom, const LLColor4 &color, S32 pixel_offset = 0, BOOL filled = TRUE );
void gl_rect_2d_offset_local( S32 left, S32 top, S32 right, S32 bottom, S32 pixel_offset = 0, BOOL filled = TRUE );
void gl_rect_2d(const LLRect& rect, BOOL filled = TRUE );
void gl_rect_2d(const LLRect& rect, const LLColor4& color, BOOL filled = TRUE );
void gl_rect_2d_checkerboard(const LLRect& parent_screen_rect, const LLRect& rect, GLfloat alpha = 1.0f);

void gl_drop_shadow(S32 left, S32 top, S32 right, S32 bottom, const LLColor4 &start_color, S32 lines);

void gl_circle_2d(F32 x, F32 y, F32 radius, S32 steps, BOOL filled);
void gl_arc_2d(F32 center_x, F32 center_y, F32 radius, S32 steps, BOOL filled, F32 start_angle, F32 end_angle);
void gl_deep_circle( F32 radius, F32 depth );
void gl_ring( F32 radius, F32 width, const LLColor4& center_color, const LLColor4& side_color, S32 steps, BOOL render_center );
void gl_corners_2d(S32 left, S32 top, S32 right, S32 bottom, S32 length, F32 max_frac);
void gl_washer_2d(F32 outer_radius, F32 inner_radius, S32 steps, const LLColor4& inner_color, const LLColor4& outer_color);
void gl_washer_segment_2d(F32 outer_radius, F32 inner_radius, F32 start_radians, F32 end_radians, S32 steps, const LLColor4& inner_color, const LLColor4& outer_color);
void gl_washer_spokes_2d(F32 outer_radius, F32 inner_radius, S32 count, const LLColor4& inner_color, const LLColor4& outer_color);

void gl_draw_image(S32 x, S32 y, LLTexture* image, const LLColor4& color = UI_VERTEX_COLOR, const LLRectf& uv_rect = LLRectf(0.f, 1.f, 1.f, 0.f));
void gl_draw_scaled_image(S32 x, S32 y, S32 width, S32 height, LLTexture* image, const LLColor4& color = UI_VERTEX_COLOR, const LLRectf& uv_rect = LLRectf(0.f, 1.f, 1.f, 0.f));
void gl_draw_rotated_image(S32 x, S32 y, F32 degrees, LLTexture* image, const LLColor4& color = UI_VERTEX_COLOR, const LLRectf& uv_rect = LLRectf(0.f, 1.f, 1.f, 0.f));
void gl_draw_scaled_rotated_image(S32 x, S32 y, S32 width, S32 height, F32 degrees,LLTexture* image, const LLColor4& color = UI_VERTEX_COLOR, const LLRectf& uv_rect = LLRectf(0.f, 1.f, 1.f, 0.f));
void gl_draw_scaled_image_with_border(S32 x, S32 y, S32 border_width, S32 border_height, S32 width, S32 height, LLTexture* image, const LLColor4 &color, BOOL solid_color = FALSE, const LLRectf& uv_rect = LLRectf(0.f, 1.f, 1.f, 0.f));
void gl_draw_scaled_image_with_border(S32 x, S32 y, S32 width, S32 height, LLTexture* image, const LLColor4 &color, BOOL solid_color = FALSE, const LLRectf& uv_rect = LLRectf(0.f, 1.f, 1.f, 0.f), const LLRectf& scale_rect = LLRectf(0.f, 1.f, 1.f, 0.f));

void gl_stippled_line_3d( const LLVector3& start, const LLVector3& end, const LLColor4& color, F32 phase = 0.f ); 

void gl_rect_2d_simple_tex( S32 width, S32 height );

// segmented rectangles

/*
   TL |______TOP_________| TR 
     /|                  |\  
   _/_|__________________|_\_
   L| |    MIDDLE        | |R
   _|_|__________________|_|_
    \ |    BOTTOM        | /  
   BL\|__________________|/ BR
      |                  |    
*/

typedef enum e_rounded_edge
{
	ROUNDED_RECT_LEFT	= 0x1, 
	ROUNDED_RECT_TOP	= 0x2, 
	ROUNDED_RECT_RIGHT	= 0x4, 
	ROUNDED_RECT_BOTTOM	= 0x8,
	ROUNDED_RECT_ALL	= 0xf
}ERoundedEdge;


void gl_segmented_rect_2d_tex(const S32 left, const S32 top, const S32 right, const S32 bottom, const S32 texture_width, const S32 texture_height, const S32 border_size, const U32 edges = ROUNDED_RECT_ALL);
void gl_segmented_rect_2d_fragment_tex(const S32 left, const S32 top, const S32 right, const S32 bottom, const S32 texture_width, const S32 texture_height, const S32 border_size, const F32 start_fragment, const F32 end_fragment, const U32 edges = ROUNDED_RECT_ALL);
void gl_segmented_rect_3d_tex(const LLVector2& border_scale, const LLVector3& border_width, const LLVector3& border_height, const LLVector3& width_vec, const LLVector3& height_vec, U32 edges = ROUNDED_RECT_ALL);
void gl_segmented_rect_3d_tex_top(const LLVector2& border_scale, const LLVector3& border_width, const LLVector3& border_height, const LLVector3& width_vec, const LLVector3& height_vec);

inline void gl_rect_2d( const LLRect& rect, BOOL filled )
{
	gl_rect_2d( rect.mLeft, rect.mTop, rect.mRight, rect.mBottom, filled );
}

inline void gl_rect_2d_offset_local( const LLRect& rect, S32 pixel_offset, BOOL filled)
{
	gl_rect_2d_offset_local( rect.mLeft, rect.mTop, rect.mRight, rect.mBottom, pixel_offset, filled );
}

// Used to hide the flashing text cursor when window doesn't have focus.
extern BOOL gShowTextEditCursor;

class LLImageProviderInterface;

typedef	void (*LLUIAudioCallback)(const LLUUID& uuid);

class LLUI
{
	LOG_CLASS(LLUI);
public:
	//
	// Methods
	//
	static void initClass(LLControlGroup* config, 
						  LLControlGroup* ignores,
						  LLControlGroup* colors, 
						  LLImageProviderInterface* image_provider,
						  LLUIAudioCallback audio_callback = NULL,
						  const LLVector2 *scale_factor = NULL,
						  const std::string& language = LLStringUtil::null);
	static void cleanupClass();

	static void pushMatrix();
	static void popMatrix();
	static void loadIdentity();
	static void translate(F32 x, F32 y, F32 z = 0.0f);

	// Return the ISO639 language name ("en", "ko", etc.) for the viewer UI.
	// http://www.loc.gov/standards/iso639-2/php/code_list.php
	static std::string getLanguage();

	//helper functions (should probably move free standing rendering helper functions here)
	static std::string locateSkin(const std::string& filename);
	static void setMousePositionScreen(S32 x, S32 y);
	static void getMousePositionScreen(S32 *x, S32 *y);
	static void setMousePositionLocal(const LLView* viewp, S32 x, S32 y);
	static void getMousePositionLocal(const LLView* viewp, S32 *x, S32 *y);
	static void setScaleFactor(const LLVector2& scale_factor);
	static void setLineWidth(F32 width);
	static LLPointer<LLUIImage> getUIImageByID(const LLUUID& image_id, S32 priority = 0);
	static LLPointer<LLUIImage> getUIImage(const std::string& name, S32 priority = 0);
	static LLVector2 getWindowSize();
	static void screenPointToGL(S32 screen_x, S32 screen_y, S32 *gl_x, S32 *gl_y);
	static void glPointToScreen(S32 gl_x, S32 gl_y, S32 *screen_x, S32 *screen_y);
	static void screenRectToGL(const LLRect& screen, LLRect *gl);
	static void glRectToScreen(const LLRect& gl, LLRect *screen);
	static void setHtmlHelp(LLHtmlHelp* html_help);

	//
	// Data
	//
	static LLControlGroup* sConfigGroup;
	static LLControlGroup* sIgnoresGroup;
	static LLControlGroup* sColorsGroup;
	static LLImageProviderInterface* sImageProvider;
	static LLUIAudioCallback sAudioCallback;
	static LLVector2		sGLScaleFactor;
	static LLWindow*		sWindow;
	static BOOL             sShowXUINames;
	static LLHtmlHelp*		sHtmlHelp;

	// *TODO: remove the following when QAR-369 settings clean-up work is in.
	// Also remove the call to this method which will then be obsolete.
	// Search for QAR-369 below to enable the proper accessing of this feature. -MG
	static void setQAMode(BOOL b);
	static BOOL sQAMode;

};

//	FactoryPolicy is a static class that controls the creation and lookup of UI elements, 
//	such as floaters.
//	The key parameter is used to provide a unique identifier and/or associated construction 
//	parameters for a given UI instance
//
//	Specialize this traits for different types, or provide a class with an identical interface 
//	in the place of the traits parameter
//
//	For example:
//
//	template <>
//	class FactoryPolicy<MyClass> /* FactoryPolicy specialized for MyClass */
//	{
//	public:
//		static MyClass* findInstance(const LLSD& key = LLSD())
//		{
//			/* return instance of MyClass associated with key */
//		}
//	
//		static MyClass* createInstance(const LLSD& key = LLSD())
//		{
//			/* create new instance of MyClass using key for construction parameters */
//		}
//	}
//	
//	class MyClass : public LLUIFactory<MyClass>
//	{
//		/* uses FactoryPolicy<MyClass> by default */
//	}

template <class T>
class FactoryPolicy
{
public:
	// basic factory methods
	static T* findInstance(const LLSD& key); // unimplemented, provide specialiation
	static T* createInstance(const LLSD& key); // unimplemented, provide specialiation
};

//	VisibilityPolicy controls the visibility of UI elements, such as floaters.
//	The key parameter is used to store the unique identifier of a given UI instance
//
//	Specialize this traits for different types, or duplicate this interface for specific instances
//	(see above)

template <class T>
class VisibilityPolicy
{
public:
	// visibility methods
	static bool visible(T* instance, const LLSD& key); // unimplemented, provide specialiation
	static void show(T* instance, const LLSD& key); // unimplemented, provide specialiation
	static void hide(T* instance, const LLSD& key); // unimplemented, provide specialiation
};

//	Manages generation of UI elements by LLSD, such that (generally) there is
//	a unique instance per distinct LLSD parameter
//	Class T is the instance type being managed, and the FACTORY_POLICY and VISIBILITY_POLICY 
//	classes provide static methods for creating, accessing, showing and hiding the associated 
//	element T
template <class T, class FACTORY_POLICY = FactoryPolicy<T>, class VISIBILITY_POLICY = VisibilityPolicy<T> >
class LLUIFactory
{
public:
	// give names to the template parameters so derived classes can refer to them
	// except this doesn't work in gcc
	typedef FACTORY_POLICY factory_policy_t;
	typedef VISIBILITY_POLICY visibility_policy_t;

	LLUIFactory()
	{
	}

 	virtual ~LLUIFactory() 
	{ 
	}

	// default show and hide methods
	static T* showInstance(const LLSD& key = LLSD()) 
	{ 
		T* instance = getInstance(key); 
		if (instance != NULL)
		{
			VISIBILITY_POLICY::show(instance, key);
		}
		return instance;
	}

	static void hideInstance(const LLSD& key = LLSD()) 
	{ 
		T* instance = getInstance(key); 
		if (instance != NULL)
		{
			VISIBILITY_POLICY::hide(instance, key);
		}
	}

	static void toggleInstance(const LLSD& key = LLSD())
	{
		if (instanceVisible(key))
		{
			hideInstance(key);
		}
		else
		{
			showInstance(key);
		}
	}

	static bool instanceVisible(const LLSD& key = LLSD())
	{
		T* instance = FACTORY_POLICY::findInstance(key);
		return instance != NULL && VISIBILITY_POLICY::visible(instance, key);
	}

	static T* getInstance(const LLSD& key = LLSD()) 
	{
		T* instance = FACTORY_POLICY::findInstance(key);
		if (instance == NULL)
		{
			instance = FACTORY_POLICY::createInstance(key);
		}
		return instance;
	}

};


//	Creates a UI singleton by ignoring the identifying parameter
//	and always generating the same instance via the LLUIFactory interface.
//	Note that since UI elements can be destroyed by their hierarchy, this singleton
//	pattern uses a static pointer to an instance that will be re-created as needed.
//	
//	Usage Pattern:
//	
//	class LLFloaterFoo : public LLFloater, public LLUISingleton<LLFloaterFoo>
//	{
//		friend class LLUISingleton<LLFloaterFoo>;
//		private:
//			LLFloaterFoo(const LLSD& key);
//	};
//	
//	Note that LLUISingleton takes an option VisibilityPolicy parameter that defines
//	how showInstance(), hideInstance(), etc. work.
// 
//  https://wiki.lindenlab.com/mediawiki/index.php?title=LLUISingleton&oldid=79352

template <class T, class VISIBILITY_POLICY = VisibilityPolicy<T> >
class LLUISingleton: public LLUIFactory<T, LLUISingleton<T, VISIBILITY_POLICY>, VISIBILITY_POLICY>
{
protected:

	// T must derive from LLUISingleton<T>
	LLUISingleton() { sInstance = static_cast<T*>(this); }

	~LLUISingleton() { sInstance = NULL; }

public:
	static T* findInstance(const LLSD& key = LLSD())
	{
		return sInstance;
	}
	
	static T* createInstance(const LLSD& key = LLSD())
	{
		if (sInstance == NULL)
		{
			sInstance = new T(key);
		}
		return sInstance;
	}

private:
	static T*	sInstance;
};

template <class T, class U> T* LLUISingleton<T,U>::sInstance = NULL;

class LLScreenClipRect
{
public:
	LLScreenClipRect(const LLRect& rect, BOOL enabled = TRUE);
	virtual ~LLScreenClipRect();

private:
	static void pushClipRect(const LLRect& rect);
	static void popClipRect();
	static void updateScissorRegion();

private:
	LLGLState		mScissorState;
	BOOL			mEnabled;

	static std::stack<LLRect> sClipRectStack;
};

class LLLocalClipRect : public LLScreenClipRect
{
public:
	LLLocalClipRect(const LLRect& rect, BOOL enabled = TRUE);
};


//RN: maybe this needs to moved elsewhere?
class LLImageProviderInterface
{
protected:
	LLImageProviderInterface() {};
	virtual ~LLImageProviderInterface() {};
public:
	virtual LLPointer<LLUIImage> getUIImage(const std::string& name, S32 priority) = 0;
	virtual LLPointer<LLUIImage> getUIImageByID(const LLUUID& id, S32 priority) = 0;
	virtual void cleanUp() = 0;
};

class LLCallbackRegistry
{
public:
	typedef boost::signals2::signal<void()> callback_signal_t;
	
	void registerCallback(const callback_signal_t::slot_type& slot)
	{
		mCallbacks.connect(slot);
	}

	void fireCallbacks()
	{
		mCallbacks();
	}

private:
	callback_signal_t mCallbacks;
};

class LLInitClassList : 
	public LLCallbackRegistry, 
	public LLSingleton<LLInitClassList>
{
	friend class LLSingleton<LLInitClassList>;
private:
	LLInitClassList() {}
};

class LLDestroyClassList : 
	public LLCallbackRegistry, 
	public LLSingleton<LLDestroyClassList>
{
	friend class LLSingleton<LLDestroyClassList>;
private:
	LLDestroyClassList() {}
};

template<typename T>
class LLRegisterWith
{
public:
	LLRegisterWith(boost::function<void ()> func)
	{
		T::instance().registerCallback(func);
	}

	// this avoids a MSVC bug where non-referenced static members are "optimized" away
	// even if their constructors have side effects
	void reference()
	{
		S32 dummy;
		dummy = 0;
	}
};

template<typename T>
class LLInitClass
{
public:
	LLInitClass() { sRegister.reference(); }

	static LLRegisterWith<LLInitClassList> sRegister;
private:

	static void initClass()
	{
		llerrs << "No static initClass() method defined for " << typeid(T).name() << llendl;
	}
};

template<typename T>
class LLDestroyClass
{
public:
	LLDestroyClass() { sRegister.reference(); }

	static LLRegisterWith<LLDestroyClassList> sRegister;
private:

	static void destroyClass()
	{
		llerrs << "No static destroyClass() method defined for " << typeid(T).name() << llendl;
	}
};

template <typename T> LLRegisterWith<LLInitClassList> LLInitClass<T>::sRegister(&T::initClass);
template <typename T> LLRegisterWith<LLDestroyClassList> LLDestroyClass<T>::sRegister(&T::destroyClass);


template <typename DERIVED>
class LLParamBlock
{
protected:
	LLParamBlock() { sBlock = (DERIVED*)this; }

	typedef typename boost::add_const<DERIVED>::type Tconst;

	template <typename T>
	class LLMandatoryParam
	{
	public:
		typedef typename boost::add_const<T>::type T_const;

		LLMandatoryParam(T_const initial_val) : mVal(initial_val), mBlock(sBlock) {}
		LLMandatoryParam(const LLMandatoryParam<T>& other) : mVal(other.mVal) {}

		DERIVED& operator ()(T_const set_value) { mVal = set_value; return *mBlock; }
		operator T() const { return mVal; } 
		T operator=(T_const set_value) { mVal = set_value; return mVal; }

	private: 
		T	mVal;
		DERIVED* mBlock;
	};

	template <typename T>
	class LLOptionalParam
	{
	public:
		typedef typename boost::add_const<T>::type T_const;

		LLOptionalParam(T_const initial_val) : mVal(initial_val), mBlock(sBlock) {}
		LLOptionalParam() : mBlock(sBlock) {}
		LLOptionalParam(const LLOptionalParam<T>& other) : mVal(other.mVal) {}

		DERIVED& operator ()(T_const set_value) { mVal = set_value; return *mBlock; }
		operator T() const { return mVal; } 
		T operator=(T_const set_value) { mVal = set_value; return mVal; }

	private:
		T	mVal;
		DERIVED* mBlock;
	};

	// specialization that requires initialization for reference types 
	template <typename T>
	class LLOptionalParam <T&>
	{
	public:
		typedef typename boost::add_const<T&>::type T_const;

		LLOptionalParam(T_const initial_val) : mVal(initial_val), mBlock(sBlock) {}
		LLOptionalParam(const LLOptionalParam<T&>& other) : mVal(other.mVal) {}

		DERIVED& operator ()(T_const set_value) { mVal = set_value; return *mBlock; }
		operator T&() const { return mVal; } 
		T& operator=(T_const set_value) { mVal = set_value; return mVal; }

	private:
		T&	mVal;
		DERIVED* mBlock;
	};

	// specialization that initializes pointer params to NULL
	template<typename T> 
	class LLOptionalParam<T*>
	{
	public:
		typedef typename boost::add_const<T*>::type T_const;

		LLOptionalParam(T_const initial_val) : mVal(initial_val), mBlock(sBlock) {}
		LLOptionalParam() : mVal((T*)NULL), mBlock(sBlock)  {}
		LLOptionalParam(const LLOptionalParam<T*>& other) : mVal(other.mVal) {}

		DERIVED& operator ()(T_const set_value) { mVal = set_value; return *mBlock; }
		operator T*() const { return mVal; } 
		T* operator=(T_const set_value) { mVal = set_value; return mVal; }
	private:
		T*	mVal;
		DERIVED* mBlock;
	};

	static DERIVED* sBlock;
};

template <typename T> T* LLParamBlock<T>::sBlock = NULL;

#endif
