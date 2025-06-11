#include "RenderApplication.h"
#include "WindowApplication.h"

int main(int, char** args)
{
	RenderApplication renderApp(800, 600, args[0]);

	return WindowApplication::Run(&renderApp);
}
