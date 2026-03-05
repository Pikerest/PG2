int main(int argc, char **argv)
{
	fps_meter FPS;

	while(!windowShouldClose(window)) {

		// Some useful work...
        //compute();
        //display();

		if (FPS.is_updated()) // display new value only once per interval (default = 1.0s)
			std::cout << "FPS: " << FPS.get() << std::endl;
            
		FPS.update();
	}

	return EXIT_SUCCESS;
}