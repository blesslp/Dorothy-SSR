/* Copyright (c) 2017 Jin Li, http://www.luvfight.me

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#pragma once

#include "Support/Geometry.h"

NS_DOROTHY_BEGIN

class TouchHandler
{
public:
	PROPERTY_BOOL(SwallowTouches);
	TouchHandler();
	virtual ~TouchHandler();
	virtual bool handle(const SDL_Event& event) = 0;
private:
	bool _swallowTouches;
};

class Node;

class Touch : public Object
{
public:
	enum
	{
		FromMouse = 1,
		FromTouch = 1 << 1,
		FromMouseAndTouch = FromMouse | FromTouch
	};
	PROPERTY_BOOL(Enabled);
	PROPERTY_READONLY(int, Id);
	PROPERTY_READONLY(Vec2, Delta);
	PROPERTY_READONLY_REF(Vec2, Location);
	PROPERTY_READONLY_REF(Vec2, PreLocation);
	PROPERTY_READONLY_REF(Vec2, WorldLocation);
	PROPERTY_READONLY_REF(Vec2, WorldPreLocation);
	static Uint32 source;
	CREATE_FUNC(Touch);
protected:
	Touch(int id);
private:
	Flag _flags;
	int _id;
	Vec2 _location;
	Vec2 _preLocation;
	Vec2 _worldLocation;
	Vec2 _worldPreLocation;
	enum
	{
		Enabled = 1,
		Selected = 1 << 1
	};
	friend class NodeTouchHandler;
	DORA_TYPE_OVERRIDE(Touch);
};

class NodeTouchHandler : public TouchHandler
{
public:
	NodeTouchHandler(Node* target);
	virtual bool handle(const SDL_Event& event) override;
protected:
	Touch* alloc(SDL_FingerID fingerId);
	Touch* get(SDL_FingerID fingerId);
	void collect(SDL_FingerID fingerId);
	Vec2 getPos(const SDL_Event& event);
	bool up(const SDL_Event& event);
	bool down(const SDL_Event& event);
	bool move(const SDL_Event& event);
private:
	Node* _target;
	stack<int> _availableTouchIds;
	unordered_map<SDL_FingerID, Ref<Touch>> _touchMap;
};

class TouchDispatcher
{
public:
	void add(const SDL_Event& event);
	void add(TouchHandler* handler);
	void dispatch();
	void clearHandlers();
	void clearEvents();
protected:
	TouchDispatcher() { }
private:
	vector<TouchHandler*> _handlers;
	list<SDL_Event> _events;
	SINGLETON_REF(TouchDispatcher, Director);
};

#define SharedTouchDispatcher \
	Dorothy::Singleton<Dorothy::TouchDispatcher>::shared()

NS_DOROTHY_END
