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
}

#endif //KISHA_ENGINE_INIT_HELPERS_HPP
