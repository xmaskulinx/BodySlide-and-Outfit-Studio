/*
Caliente's BodySlide
by Caliente

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/

#include "PreviewWindow.h"
#include "BodySlideApp.h"

PreviewWindow::~PreviewWindow() {
}

const char* PreviewWindow::GetTitle(char previewType) {
	if (previewType == SMALL_PREVIEW)
		return "Preview: Small Size";
	return "Preview: Large Size";
}

wxSize PreviewWindow::GetDefaultSize() {
	int xborder = wxSystemSettings::GetMetric(wxSYS_FRAMESIZE_X);
	if (xborder < 0)
		xborder = 0;
	int yborder = wxSystemSettings::GetMetric(wxSYS_FRAMESIZE_Y);
	if (yborder < 0)
		yborder = 0;
	return wxSize(768 + xborder * 2, 768 + yborder * 2);
}

PreviewWindow::PreviewWindow(BodySlideApp* a, char previewType)
	: wxFrame(nullptr, wxID_ANY, GetTitle(previewType), wxDefaultPosition, GetDefaultSize()), app(a), isSmall(previewType == SMALL_PREVIEW) {
	SetIcon(wxIcon("res\\outfitstudio.png", wxBITMAP_TYPE_PNG));
	canvas = new PreviewCanvas(this, GLSurface::GetGLAttribs(this));
	context = new wxGLContext(canvas);
	Show();
}

void PreviewWindow::OnShown() {
	gls.Initialize(canvas, context);
	auto size = canvas->GetSize();
	gls.SetStartingView(Vector3(0.0f, -5.0f, -15.0f), Vector3(15.0f, 0.0f, 0.0f), size.GetWidth(), size.GetHeight());

	int ambient = Config.GetIntValue("Lights/Ambient");
	int brightness1 = Config.GetIntValue("Lights/Brightness1");
	int brightness2 = Config.GetIntValue("Lights/Brightness2");
	int brightness3 = Config.GetIntValue("Lights/Brightness3");
	gls.UpdateLights(ambient, brightness1, brightness2, brightness3);

	app->InitPreview(isSmall ? SMALL_PREVIEW : BIG_PREVIEW);
}

void PreviewWindow::AddMeshDirect(mesh* m) {
	gls.AddMeshDirect(m);
}

void PreviewWindow::AddMeshFromNif(NifFile *nif, char *shapeName) {
	vector<string> shapeList;
	nif->GetShapeList(shapeList);
	mesh* m = nullptr;
	for (int i = 0; i < shapeList.size(); i++) {
		if (shapeName && (shapeList[i] == shapeName)) {
			gls.AddMeshFromNif(nif, shapeList[i]);
			m = gls.GetMesh(shapeName);
			m->BuildTriAdjacency();
			if (m->smoothSeamNormals) {
				NiShader* shader = nif->GetShader(shapeList[i]);
				if (shader && !shader->IsSkin())
					ToggleSmoothSeams(m);
			}
		}
		else if (shapeName) {
			continue;
		}
		else {
			gls.AddMeshFromNif(nif, shapeList[i]);
			m = gls.GetMesh(shapeList[i]);
			m->BuildTriAdjacency();
			if (m->smoothSeamNormals) {
				NiShader* shader = nif->GetShader(shapeList[i]);
				if (shader && !shader->IsSkin())
					ToggleSmoothSeams(m);
			}
		}
	}
}

void PreviewWindow::RefreshMeshFromNif(NifFile* nif, char* shapeName) {
	vector<string> shapeList;
	nif->GetShapeList(shapeList);
	mesh* m = nullptr;
	if (shapeName == nullptr)
		gls.DeleteAllMeshes();

	for (int i = 0; i < shapeList.size(); i++) {
		if (shapeName && (shapeList[i] == shapeName)) {
			gls.ReloadMeshFromNif(nif, shapeList[i]);
			m = gls.GetMesh(shapeName);
			m->BuildTriAdjacency();
			if (m->smoothSeamNormals) {
				NiShader* shader = nif->GetShader(shapeList[i]);
				if (shader && !shader->IsSkin())
					ToggleSmoothSeams(m);
			}
			auto iter = shapeTextures.find(shapeName);
			if (iter != shapeTextures.end())
				m->material = iter->second;
			else
				AddNifShapeTexture(nif, string(shapeName));

		}
		else if (shapeName) {
			continue;
		}
		else {
			gls.ReloadMeshFromNif(nif, shapeList[i]);
			m = gls.GetMesh(shapeList[i]);
			m->BuildTriAdjacency();
			if (m->smoothSeamNormals) {
				NiShader* shader = nif->GetShader(shapeList[i]);
				if (shader && !shader->IsSkin())
					ToggleSmoothSeams(m);
			}
			auto iter = shapeTextures.find(shapeList[i]);
			if (iter != shapeTextures.end())
				m->material = iter->second;
			else
				AddNifShapeTexture(nif, shapeList[i]);
		}
	}
	Refresh();
}

void PreviewWindow::AddNifShapeTexture(NifFile* fromNif, const string& shapeName) {
	string texFile;
	fromNif->GetTextureForShape(shapeName, texFile, 0);

	bool isSkin = false;
	NiShader* shader = fromNif->GetShader(shapeName);
	if (shader && shader->IsSkin())
		isSkin = true;

	SetShapeTexture(shapeName, baseDataPath + texFile, isSkin);
}

void PreviewWindow::RightDrag(int dX, int dY) {
	gls.TurnTableCamera(dX);
	gls.PitchCamera(dY);
	Refresh();
}

void PreviewWindow::LeftDrag(int dX, int dY) {
	gls.PanCamera(dX, dY);
	Refresh();
}

void PreviewWindow::TrackMouse(int X, int Y) {
	gls.UpdateCursor(X, Y);
	Refresh();
}

void PreviewWindow::MouseWheel(int dW) {
	gls.DollyCamera(dW);
	Refresh();
}

void PreviewWindow::Pick(int X, int Y) {
	Vector3 camVec;
	Vector3 dirVec;
	float len;
	gls.GetPickRay(X, Y, dirVec, camVec);
	len = sqrt(camVec.x*camVec.x + camVec.y*camVec.y + camVec.z*camVec.z);

	gls.AddVisRay(camVec, dirVec, len);
	Refresh();
}

void PreviewWindow::OnClose(wxCloseEvent& WXUNUSED(event)) {
	Destroy();
	canvas = nullptr;
	delete context;
	app->PreviewClosed(isSmall ? SMALL_PREVIEW : BIG_PREVIEW);
}

BEGIN_EVENT_TABLE(PreviewWindow, wxFrame)
	EVT_CLOSE(PreviewWindow::OnClose)
END_EVENT_TABLE();

PreviewCanvas::PreviewCanvas(PreviewWindow* pw, const int* attribs)
	: wxGLCanvas(pw, wxID_ANY, attribs, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE),
		previewWindow(pw) {
}

void PreviewCanvas::OnPaint(wxPaintEvent& WXUNUSED(event)) {
	// Initialize OpenGL the first time the window is painted.
	// We unfortunately can't initialize it before the window is shown.
	// We could register for the EVT_SHOW event, but unfortunately it
	// appears to only be called after the first few EVT_PAINT events.
	// It also isn't supported on all platforms.
	if (firstPaint) {
		firstPaint = false;
		previewWindow->OnShown();
	}
	previewWindow->Render();
}

void PreviewCanvas::OnKeyUp(wxKeyEvent& event) {
	int key = event.GetKeyCode();
	switch (key) {
	case 'N':
		previewWindow->ToggleSmoothSeams();
		break;
	case 'T':
		previewWindow->ToggleTextures();
		break;
	case 'W':
		previewWindow->ToggleWireframe();
		break;
	case 'L':
		previewWindow->ToggleLighting();
		break;
	case 'Q':
		previewWindow->ToggleEditMode();
		break;
	}
}

void PreviewCanvas::OnMotion(wxMouseEvent& event) {
	if (event.LeftIsDown()) {
		auto delta = event.GetPosition() - lastMousePosition;
		previewWindow->LeftDrag(delta.x, delta.y);
	}
	if (event.RightIsDown()) {
		auto delta = event.GetPosition() - lastMousePosition;
		previewWindow->RightDrag(delta.x, delta.y);
	}
	if (!event.LeftIsDown() && !event.RightIsDown()) {
		previewWindow->TrackMouse(event.GetX(), event.GetY());
	}
	lastMousePosition = event.GetPosition();
}

void PreviewCanvas::OnMouseWheel(wxMouseEvent& event) {
	previewWindow->MouseWheel(event.GetWheelRotation());
}

void PreviewCanvas::OnResized(wxSizeEvent& event) {
	previewWindow->Resized(event.GetSize().GetWidth(), event.GetSize().GetHeight());
}

BEGIN_EVENT_TABLE(PreviewCanvas, wxGLCanvas)
	EVT_KEY_UP(PreviewCanvas::OnKeyUp)
	EVT_MOTION(PreviewCanvas::OnMotion)
	EVT_MOUSEWHEEL(PreviewCanvas::OnMouseWheel)
	EVT_PAINT(PreviewCanvas::OnPaint)
	EVT_SIZE(PreviewCanvas::OnResized)
END_EVENT_TABLE();
