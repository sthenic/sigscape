# Some helper targets

BUILD_DIRECTORY=build
all: generate
	cmake --build ${BUILD_DIRECTORY}

generate:
	cmake -B ${BUILD_DIRECTORY} -S .

clean:
	cmake --build ${BUILD_DIRECTORY} --target clean

wipe:
	rm -rf ${BUILD_DIRECTORY}
