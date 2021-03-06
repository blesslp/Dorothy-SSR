/* Copyright (c) 2017 Jin Li, http://www.luvfight.me

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "Const/Header.h"
#include "Node/ClipNode.h"
#include "Basic/Renderer.h"
#include "Basic/Application.h"
#include "Cache/ShaderCache.h"
#include "Effect/Effect.h"
#include "Node/Sprite.h"
#include "Basic/View.h"

NS_DOROTHY_BEGIN

int ClipNode::_layer = -1;

ClipNode::ClipNode(Node* stencil):
_alphaThreshold(1.0f),
_stencil(stencil)
{ }

ClipNode::~ClipNode()
{ }

void ClipNode::setStencil(Node* var)
{
	AssertIf(var && var->getParent(), "stencil node already added. It can't be added again.");
	if (_stencil)
	{
		_stencil->onExit();
	}
	_stencil = var;
	if (_stencil)
	{
		_stencil->setTransformTarget(this);
		setupAlphaTest();
		if (isRunning())
		{
			_stencil->onEnter();
		}
	}
}

Node* ClipNode::getStencil() const
{
	return _stencil;
}

void ClipNode::setAlphaThreshold(float var)
{
	_alphaThreshold = var;
	setupAlphaTest();
}

float ClipNode::getAlphaThreshold() const
{
	return _alphaThreshold;
}

void ClipNode::setInverted(bool var)
{
	_flags.setFlag(ClipNode::Inverted, var);
}

bool ClipNode::isInverted() const
{
	return _flags.isOn(ClipNode::Inverted);
}

bool ClipNode::init()
{
	if (_stencil)
	{
		_stencil->setTransformTarget(this);
	}
	return true;
}

void ClipNode::onEnter()
{
	Node::onEnter();
	if (_stencil)
	{
		_stencil->onEnter();
	}
}

void ClipNode::onExit()
{
	Node::onExit();
	if (_stencil)
	{
		_stencil->onExit();
	}
}

void ClipNode::cleanup()
{
	Node::cleanup();
	if (_stencil)
	{
		_stencil->cleanup();
	}
}

void ClipNode::drawFullScreenStencil(Uint8 maskLayer, bool value)
{
	SharedRendererManager.flush();
	bgfx::TransientVertexBuffer vertexBuffer;
	bgfx::TransientIndexBuffer indexBuffer;
	if (bgfx::allocTransientBuffers(&vertexBuffer, PosColorVertex::ms_decl, 4, &indexBuffer, 6))
	{
		float width = s_cast<float>(SharedApplication.getWidth());
		float height = s_cast<float>(SharedApplication.getHeight());
		Vec4 pos[4] = {
			{0, height, 0, 1},
			{width, height, 0, 1},
			{0, 0, 0, 1},
			{width, 0, 0, 1}
		};
		PosColorVertex* vertices = r_cast<PosColorVertex*>(vertexBuffer.data);
		Matrix ortho;
		bx::mtxOrtho(ortho, 0, width, 0, height, 0, 1000.0f);
		for (int i = 0; i < 4; i++)
		{
			bx::vec4MulMtx(&vertices[i].x, pos[i], ortho);
		}
		const uint16_t indices[] = {0, 1, 2, 1, 3, 2};
		std::memcpy(indexBuffer.data, indices, sizeof(uint16_t) * 6);
		Uint32 func = BGFX_STENCIL_TEST_NEVER |
			BGFX_STENCIL_FUNC_REF(maskLayer) | BGFX_STENCIL_FUNC_RMASK(maskLayer);
		Uint32 fail = value ? BGFX_STENCIL_OP_FAIL_S_REPLACE : BGFX_STENCIL_OP_FAIL_S_ZERO;
		Uint32 op = fail | BGFX_STENCIL_OP_FAIL_Z_KEEP | BGFX_STENCIL_OP_PASS_Z_KEEP;
		Uint32 stencil = func | op;
		bgfx::setStencil(stencil);
		bgfx::setVertexBuffer(&vertexBuffer);
		bgfx::setIndexBuffer(&indexBuffer);
		bgfx::setState(BGFX_STATE_NONE);
		Uint8 viewId = SharedView.getId();
		bgfx::submit(viewId, SharedLineRenderer.getDefaultEffect()->apply());
	}
}

void ClipNode::drawStencil(Uint8 maskLayer, bool value)
{
	Uint32 func = BGFX_STENCIL_TEST_NEVER |
		BGFX_STENCIL_FUNC_REF(maskLayer) | BGFX_STENCIL_FUNC_RMASK(maskLayer);
	Uint32 fail = value ? BGFX_STENCIL_OP_FAIL_S_REPLACE : BGFX_STENCIL_OP_FAIL_S_ZERO;
	Uint32 op = fail | BGFX_STENCIL_OP_FAIL_Z_KEEP | BGFX_STENCIL_OP_PASS_Z_KEEP;
	SharedRendererManager.pushStencilState(func | op, [&]()
	{
		_stencil->visit();
		SharedRendererManager.flush();
	});
}

void ClipNode::setupAlphaTest()
{
	if (_stencil)
	{
		bool setup = _alphaThreshold < 1.0f;
		SpriteEffect* effect = setup ? SharedSpriteRenderer.getAlphaTestEffect() : SharedSpriteRenderer.getDefaultEffect();
		_stencil->traverse([effect, this](Node* node)
		{
			Sprite* sprite = DoraCast<Sprite>(node);
			if (sprite)
			{
				sprite->setEffect(effect);
				sprite->setAlphaRef(_alphaThreshold);
			}
			return false;
		});
	}
}

void ClipNode::visit()
{
	if (!_stencil || !_stencil->isVisible())
	{
		if (_flags.isOn(ClipNode::Inverted))
		{
			Node::visit();
		}
		return;
	}
	if (_layer + 1 == 8)
	{
		static bool once = true;
		if (once)
		{
			once = false;
			Log("Nesting more than %d stencils is not supported. Everything will be drawn without stencil for this node and its childs.", 8);
		}
		Node::visit();
		return;
	}
	_layer++;
	Uint32 maskLayer = 1 << _layer;
	Uint32 maskLayerLess = maskLayer - 1;
	Uint32 maskLayerLessEqual = maskLayer | maskLayerLess;
	if (isInverted())
	{
		drawFullScreenStencil(maskLayer, true);
	}
	drawStencil(maskLayer, !isInverted());
	Uint32 func = BGFX_STENCIL_TEST_EQUAL | BGFX_STENCIL_FUNC_REF(maskLayerLessEqual) | BGFX_STENCIL_FUNC_RMASK(maskLayerLessEqual);
	Uint32 op = BGFX_STENCIL_OP_FAIL_S_KEEP | BGFX_STENCIL_OP_FAIL_Z_KEEP | BGFX_STENCIL_OP_PASS_Z_KEEP;
	SharedRendererManager.pushStencilState(func | op, [&]()
	{
		Node::visit();
		SharedRendererManager.flush();
	});
	if (isInverted())
	{
		drawFullScreenStencil(maskLayer, false);
	}
	else
	{
		drawStencil(maskLayer, false);
	}
	_layer--;
}

NS_DOROTHY_END
