# ImageNet console and camera test programs
cuda_add_executable(imagenet_console imagenet_console.cpp)
target_link_libraries(imagenet_console tensorflowrt_util tensorflowrt)

cuda_add_executable(imagenet_camera imagenet_camera.cpp)
target_link_libraries(imagenet_camera tensorflowrt_util tensorflowrt X11)

# Installation
install(TARGETS imagenet_console DESTINATION bin)
install(TARGETS imagenet_camera DESTINATION bin)
