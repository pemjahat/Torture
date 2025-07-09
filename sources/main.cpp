#include "RenderApplication.h"
#include "WindowApplication.h"

int main(int argc, char** argv)
{
	RenderApplication renderApp(argc, argv);

	return WindowApplication::Run(&renderApp);
}
