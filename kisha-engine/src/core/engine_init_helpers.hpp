//
// Created by jsumihiro on 6/12/26.
//

#ifndef KISHA_ENGINE_INIT_HELPERS_HPP
#define KISHA_ENGINE_INIT_HELPERS_HPP

namespace kisha::engine::util {

  void append_unique(std::vector<std::string> *names, std::string_view name);
  std::vector<const char *> to_c_string_ptrs(const std::vector<std::string> &names);
  std::vector<std::string> enumerate_instance_extension_names(const vk::raii::Context &context);
  std::vector<std::string> enumerate_instance_layer_names(const vk::raii::Context &context);
  std::vector<std::string> enumerate_device_extension_names(const vk::raii::PhysicalDevice &physical_device);
  [[nodiscard]] std::expected<void, MissingNamesError> validate_required_names(const std::vector<std::string> &available, const std::vector<std::string> &required);
  [[nodiscard]] std::expected<vk::raii::Instance, EngineInitError> create_instance(const vk::raii::Context &context,
                                                                                   const vk::ApplicationInfo &application_info,
                                                                                   const std::vector<std::string> &required_layers,
                                                                                   const std::vector<std::string> &required_extensions);
  VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                       const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);
}

#endif //KISHA_ENGINE_INIT_HELPERS_HPP
