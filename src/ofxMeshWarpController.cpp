#include "ofxMeshWarpController.h"
#include "ofGraphics.h"

using namespace ofx::MeshWarp;
using namespace ofx::MeshWarp::Editor;

Controller::Controller()
{
//	enable();
}
Controller::~Controller()
{
	disable();
}
void Controller::enable()
{
	if(!is_enabled_) {
		ofRegisterMouseEvents(this);
		ofRegisterKeyEvents(this);
		is_enabled_ = true;
	}
}
void Controller::disable()
{
	if(is_enabled_) {
		ofUnregisterMouseEvents(this);
		ofUnregisterKeyEvents(this);
		mouse_op_.hover = nullptr;
		mouse_op_.inside_rect.clear();
		mouse_op_.edit.clear();
		selected_.clear();
		mouse_op_.pressed_state = MouseOperation::STATE_NONE;
		is_enabled_ = false;
	}
}
void Controller::add(Mesh *mesh)
{
	meshes_.insert(mesh);
}
void Controller::clear()
{
	mouse_op_.hover = nullptr;
	mouse_op_.inside_rect.clear();
	mouse_op_.edit.clear();
	selected_.clear();
	mouse_op_.pressed_state = MouseOperation::STATE_NONE;
	meshes_.clear();
}
void Controller::draw()
{
	auto drawCircle = [&](MeshPoint* p, float size_add=0) {
		float size = point_size_*(p->isNode()?1:0.5f)+size_add;
		ofDrawCircle(p->point(), size);
	};
	ofPushStyle();
	for(auto &mesh : meshes_) {
		mesh->drawWireframe();
	}
	ofSetColor(ofColor::green);
	ofNoFill();
	for(auto &mesh : meshes_) {
		auto points = mesh->getPoints();
		for(auto &p : points) {
			drawCircle(p);
		}
	}
	ofSetColor(ofColor::green);
	ofFill();
	if(mouse_op_.hover) {
		drawCircle(mouse_op_.hover, 2);
	}
	for(auto &p : mouse_op_.inside_rect) {
		drawCircle(p, 2);
	}
	ofSetColor(ofColor::red);
	ofFill();
	for(auto &p : selected_) {
		drawCircle(p);
	}
	for(auto &p : mouse_op_.edit) {
		drawCircle(p.first);
	}
	if(isMakingRect()) {
		ofFill();
		ofSetColor(ofColor::white, 128);
		ofDrawRectangle(ofRectangle(mouse_op_.pressed_pos, mouse_op_.pos));
	}
	ofPopStyle();
}
void Controller::mousePressed(ofMouseEventArgs &args)
{
	mouse_op_.pressed_pos = args;
	mouse_op_.pressed_state = MouseOperation::STATE_NONE;
	switch(args.button) {
		case OF_MOUSE_BUTTON_LEFT:
			if(mouse_op_.hover) {
				mouse_op_.pressed_state = MouseOperation::STATE_GRABBING;
			}
			else {
				mouse_op_.pressed_state = MouseOperation::STATE_MAKING_RECT;
			}
			break;
		case OF_MOUSE_BUTTON_MIDDLE:
			if(mouse_op_.hover) {
				mouse_op_.pressed_state = MouseOperation::STATE_GRABBING;
			}
			break;
	}
	switch(mouse_op_.pressed_state) {
		case MouseOperation::STATE_GRABBING: {
			const auto &it = selected_.insert(mouse_op_.hover);
			if(it.second) {
				if(!isAlternative() && !isAdditive()) {
					selected_.clear();
					selected_.insert(mouse_op_.hover);
				}
			}
			else {
				if(isAlternative()) {
					selected_.erase(mouse_op_.hover);
					mouse_op_.pressed_state = MouseOperation::STATE_NONE;
				}
			}
			if(isToggleNode()) {
				bool set = !mouse_op_.hover->isNode();
				mouse_op_.hover->setNode(set);
				for(auto &p : selected_) {
					p->setNode(set);
				}
				mouse_op_.pressed_state = MouseOperation::STATE_NONE;
			}
			else if(isGrabbing()) {
				for(auto &p : selected_) {
					mouse_op_.edit.push_back(p);
				}
				mouse_op_.hover = nullptr;
			}
		}	break;
		default:
			break;
	}
}
void Controller::mouseReleased(ofMouseEventArgs &args)
{
	if(isMakingRect()) {
		if(!isAlternative() && !isAdditive()) {
			selected_.clear();
		}
		for(auto &p : mouse_op_.inside_rect) {
			const auto &it = selected_.insert(p);
			if(isAlternative() && !it.second) {
				selected_.erase(it.first);
			}
		}
		mouse_op_.inside_rect.clear();
	}
	else {
		mouse_op_.edit.clear();
	}
	mouse_op_.pressed_state = MouseOperation::STATE_NONE;
	mouse_op_.hover = getHit(args);
}
void Controller::mouseMoved(ofMouseEventArgs &args)
{
	mouse_op_.pos = args;
	mouse_op_.hover = getHit(args);
}
MeshPoint* Controller::getHit(const ofVec2f &test) const
{
	for(auto &mesh : meshes_) {
		MeshHelper m(mesh);
		if(MeshPoint *hover = MeshHelper(mesh).getHit(test, point_size_)) {
			return hover;
		}
	}
	return nullptr;
}

void Controller::mouseDragged(ofMouseEventArgs &args)
{
	mouse_op_.pos = args;
	if(isMakingRect()) {
		mouse_op_.inside_rect.clear();
		ofRectangle rect(mouse_op_.pressed_pos, args);
		for(auto &mesh : meshes_) {
			const auto &hit = MeshHelper(mesh).getHit(rect);
			mouse_op_.inside_rect.insert(mouse_op_.inside_rect.end(), hit.begin(), hit.end());
		}
	}
	else if(isGrabbing()) {
		ofVec2f delta = args-mouse_op_.pressed_pos;
		if(isSlide()) {
			delta.x = abs(delta.x)<abs(delta.y)?0:delta.x;
			delta.y = abs(delta.y)<abs(delta.x)?0:delta.y;
		}
		bool moved_any = false;
		for(auto &work : mouse_op_.edit) {
			if(work.first->isNode()) {
				if(isEditVertex()) {
					work.setVertexMove(delta);
					work.resetCoord();
				}
				if(isEditCoord()) {
					work.setCoordMove(delta*screen_to_coord_);
					work.resetVertex();
				}
				moved_any = true;
			}
		}
		if(moved_any) {
			for(auto &m : meshes_) {
				m->solve();
			}
		}
	}
}
void Controller::mouseScrolled(ofMouseEventArgs &args)
{
}
void Controller::mouseEntered(ofMouseEventArgs &args)
{
}
void Controller::mouseExited(ofMouseEventArgs &args)
{
}
void Controller::keyPressed(ofKeyEventArgs &args)
{
	ofVec2f delta;
	switch(args.key) {
		case OF_KEY_UP:		delta = ofVec2f(0,-1); break;
		case OF_KEY_DOWN:	delta = ofVec2f(0, 1); break;
		case OF_KEY_LEFT:	delta = ofVec2f(-1,0); break;
		case OF_KEY_RIGHT:	delta = ofVec2f(1, 0); break;
	}
	if(delta.lengthSquared() > 0) {
		bool moved_any = false;
		for(auto &p : selected_) {
			if(p->isNode()) {
				if(isEditVertex()) {
					PointHelper(p).moveVertex(delta*(isArrowKeyJump()?10:1));
				}
				if(isEditCoord()){
					PointHelper(p).moveCoord(delta*(isArrowKeyJump()?10:1)*screen_to_coord_);
				}
				moved_any = true;
			}
		}
		if(moved_any) {
			for(auto &m : meshes_) {
				m->solve();
			}
		}
		mouse_op_.hover = getHit(mouse_op_.pos);
	}
}
void Controller::keyReleased(ofKeyEventArgs &args)
{
}


MeshPoint* MeshHelper::getHit(const ofVec2f &test, float room, int index) const
{
	const vector<MeshPoint*> &points = target_->getPoints();
	for(auto &p : points) {
		if(index < 0) {
			return nullptr;
		}
		if(test.squareDistance(p->point()) < room*room && index-- == 0) {
			return p;
		}
	}
	return nullptr;
}
vector<MeshPoint*> MeshHelper::getHit(const ofRectangle &test) const
{
	vector<MeshPoint*> ret;
	const vector<MeshPoint*> &points = target_->getPoints();
	for(auto &p : points) {
		if(test.inside(p->point())) {
			ret.push_back(p);
		}
	}
	return ret;
}
