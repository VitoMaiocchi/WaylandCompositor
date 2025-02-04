#define LOGGER_CATEGORY Logger::SURFACE
#include "surface.hpp"
#include "config.hpp"
#include "layout.hpp"
#include "input.hpp"

#include <cassert>

namespace Surface {

//BASE CLASS
bool Base::contains(int x, int y) {
	if(extends.x > x) return false;
	if(extends.x + extends.width < x) return false;
	if(extends.y > y) return false;
	if(extends.y + extends.height < y) return false;
	return true;
}

void Base::setExtends(wlr_box extends) {
	bool resize = this->extends.width != extends.width || this->extends.height != extends.height;
	this->extends = extends;
	extendsUpdateNotify(resize);
}

//PARENT
wlr_scene_tree* Parent::addChild(Child* child) {
	assert(child);
	assert(children.find(child) == children.end());
	children.insert(child);
	//this is not the most efficient and elegant solution but its good enough for now
	arrangeAll();

	return root_node;
}

void Parent::removeChild(Child* child) {
	auto it = children.find(child);
	assert(it != children.end()); //can not remove a child that has not been added
	children.erase(it);
}

bool Parent::contains(int x, int y, bool include_children) {
	if(!include_children) return Base::contains(x,y);
	if(Base::contains(x,y)) return true;
	x -= extends.x;
	y -= extends.y;
	for(auto c : children) if(c->contains(x,y,true)) return true;
	return false;
}

void Parent::arrangeChildren(Extends ext, int x, int y) {
	//ext = transform(ext, extends);
	for(Child* c : children) {
		c->arrange(ext, x, y);
		c->arrangeChildren(ext, x, y);
	}
}


//CHILD
void Child::arrangeAll() {
		parent->arrangeAll();
}


//TOPLEVEL
void Toplevel::arrangeAll() {
	arrangeChildren(*child_ext, extends.x, extends.y);
}

void Toplevel::setChildExtends(Extends* ext) {
	child_ext = ext;
	arrangeAll();
}

Toplevel::Toplevel() {
	for(uint i = 0; i < 4; i++) border[i] = NULL;
	extends = {0,0,0,0};
	visible = false;
	focused = false;
	child_ext = &extends;
}

void Toplevel::setFocus(bool focus) {
	//FIXME: remove keyboard wenn unfocus
	if(focus == focused) return;
	focused = focus;
	if(!visible) return;

	setActivated(focus);
	if(!focus) {
		for(int i = 0; i < 4; i++) wlr_scene_rect_set_color(border[i], bordercolor_inactive);
		return;
	}

	for(int i = 0; i < 4; i++) wlr_scene_rect_set_color(border[i], bordercolor_active);
	Input::setKeyboardFocus(this);
	wlr_scene_node_raise_to_top(&root_node->node);
}

void Toplevel::setVisibility(bool visible) {
	if(this->visible == visible) return;
	this->visible = visible;
	
	if(!visible) {
		wlr_scene_node_set_enabled(&root_node->node, false);
		return;
	}
	
	wlr_scene_node_set_position(&root_node->node, extends.x, extends.y);
	debug("Toplevel set suface size: width={}, height={}", extends.width, extends.height);
	assert(extends.width > 0 && extends.height > 0);
	//FIXME: surface size edge case
	setSurfaceSize(extends.width-2*BORDERWIDTH, extends.height-2*BORDERWIDTH);
	wlr_scene_node_set_position(&surface_node->node, BORDERWIDTH, BORDERWIDTH);

	wlr_scene_node_set_enabled(&root_node->node, true);

	if(!border[0]) { //create window decorations if they don't allready exist
		auto color = focused? bordercolor_active : bordercolor_inactive;
		border[0] = wlr_scene_rect_create(root_node, extends.width, BORDERWIDTH, color);
		border[1] = wlr_scene_rect_create(root_node, extends.width, BORDERWIDTH, color);
		border[2] = wlr_scene_rect_create(root_node, BORDERWIDTH, extends.height-2*BORDERWIDTH, color);
		border[3] = wlr_scene_rect_create(root_node, BORDERWIDTH, extends.height-2*BORDERWIDTH, color);

		wlr_scene_node_set_position(&border[1]->node, 0, extends.height-BORDERWIDTH);
		wlr_scene_node_set_position(&border[2]->node, 0, BORDERWIDTH);
		wlr_scene_node_set_position(&border[3]->node, extends.width-BORDERWIDTH, BORDERWIDTH);
	}

	setActivated(focused);
	if(focused) {
		Input::setKeyboardFocus(this);
		wlr_scene_node_raise_to_top(&root_node->node);
	}
}

std::pair<int, int> Toplevel::surfaceCoordinateTransform(int x, int y) const {
	return {x - (extends.x + BORDERWIDTH), y - (extends.y + BORDERWIDTH)};
}

void Toplevel::mapNotify(bool mapped) {
	if(mapped) {
		Layout::addSurface(this);
		return;
	} 

	Layout::removeSurface(this);

	for(unsigned i = 0; i < 4; i++) {
		assert(border[i]);
		wlr_scene_node_destroy(&border[i]->node);
		border[i] = NULL;
	}
}

void Toplevel::extendsUpdateNotify(bool resize) {
	if(!visible) return;

	//UPDATE WINDOW DECORATION
	wlr_scene_node_set_position(&root_node->node, extends.x, extends.y);
	//TODO: possibly rearrange children
	if(!resize) return; //the rest only involves resize

	setSurfaceSize(extends.width-2*BORDERWIDTH, extends.height-2*BORDERWIDTH);
	
	wlr_scene_node_set_position(&border[1]->node, 0, extends.height-BORDERWIDTH);
	wlr_scene_node_set_position(&border[3]->node, extends.width-BORDERWIDTH, BORDERWIDTH);

	wlr_scene_rect_set_size(border[0], extends.width, BORDERWIDTH);
	wlr_scene_rect_set_size(border[1], extends.width, BORDERWIDTH);
	wlr_scene_rect_set_size(border[2], BORDERWIDTH, extends.height-2*BORDERWIDTH);
	wlr_scene_rect_set_size(border[3], BORDERWIDTH, extends.height-2*BORDERWIDTH);
}

}