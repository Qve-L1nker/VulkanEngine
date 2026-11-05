cc = g++
CFLAGS = -std=c++17 -static -O2

INCLUDE_DIR = -I../ThirdParty/Vulkan/Include -I../ThirdParty/SDL3-3.4.0/include -I../ThirdParty/SDL3_ttf-3.2.2/include -I../ThirdParty/SDL3_image-3.4.0/include -I../ThirdParty/assimp-6.0.5/include -I../VulkanEngine
LIB_DIR = -L../ThirdParty/Vulkan/Lib -L../ThirdParty/SDL3-3.4.0/lib/x64 -L../ThirdParty/SDL3_ttf-3.2.2/lib/x64 -L../ThirdParty/SDL3_image-3.4.0/lib/x64 -L../ThirdParty/assimp-6.0.5/lib/x64
LIB = -lSDL3 -lSDL3_image -lSDL3_ttf -lassimp-vc145-mt -lvulkan-1

SRC_PATH = ../VulkanEngine/

ALL_OBJ = main.o \
		VulkanEngine.o \
		VkRenderer.o \
		Model.o \
		TestModel.o \
		Camera.o \
		Animation.o \
		ResourcesManager.o

vulkan: $(ALL_OBJ)
	$(cc) $(ALL_OBJ) $(CFLAGS) $(LIB_DIR) $(LIB) -o ../VulkanEngine/VulkanEngine.exe

main.o: $(SRC_PATH)main.cpp
	$(cc) $(SRC_PATH)main.cpp $(CFLAGS) $(INCLUDE_DIR) -c -o main.o

VulkanEngine.o: $(SRC_PATH)VulkanEngine.cpp
	$(cc) $(SRC_PATH)VulkanEngine.cpp $(CFLAGS) $(INCLUDE_DIR) -c -o VulkanEngine.o

VkRenderer.o: $(SRC_PATH)VkRenderer.cpp
	$(cc) $(SRC_PATH)VkRenderer.cpp $(CFLAGS) $(INCLUDE_DIR) -c -o VkRenderer.o

Model.o: $(SRC_PATH)Model.cpp
	$(cc) $(SRC_PATH)Model.cpp $(CFLAGS) $(INCLUDE_DIR) -c -o Model.o

TestModel.o: $(SRC_PATH)TestModel.cpp
	$(cc) $(SRC_PATH)TestModel.cpp $(CFLAGS) $(INCLUDE_DIR) -c -o TestModel.o

Camera.o: $(SRC_PATH)Camera.cpp
	$(cc) $(SRC_PATH)Camera.cpp $(CFLAGS) $(INCLUDE_DIR) -c -o Camera.o

Animation.o: $(SRC_PATH)Animation.cpp
	$(cc) $(SRC_PATH)Animation.cpp $(CFLAGS) $(INCLUDE_DIR) -c -o Animation.o

ResourcesManager.o: $(SRC_PATH)ResourcesManager.cpp
	$(cc) $(SRC_PATH)ResourcesManager.cpp $(CFLAGS) $(INCLUDE_DIR) -c -o ResourcesManager.o

.PHONY: clean

clean:
	del .\*.o
	del ..\VulkanEngine\VulkanEngine.exe