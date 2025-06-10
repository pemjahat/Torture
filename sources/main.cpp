#include "RenderApplication.h"
#include "WindowApplication.h"

int main(int, char**)
{
	RenderApplication renderApp(800, 600);

	return WindowApplication::Run(&renderApp);
}
