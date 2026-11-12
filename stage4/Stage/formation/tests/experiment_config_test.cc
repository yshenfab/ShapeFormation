#include "../experiment_config.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << std::endl;
    std::exit(EXIT_FAILURE);
  }
}

void expect_near(double actual,
                 double expected,
                 double tolerance,
                 const std::string& message) {
  if (std::fabs(actual - expected) > tolerance) {
    std::cerr << "FAIL: " << message << " (expected " << expected
              << ", got " << actual << ')' << std::endl;
    std::exit(EXIT_FAILURE);
  }
}

void test_fixed_density_uses_protocol_side_by_default() {
  const formation::ArenaResolution result =
      formation::resolve_arena_side(500, "", "");
  expect_true(result.success, "fixed-density default must be accepted");
  expect_true(!result.qualitative, "default mode must remain statistical");
  expect_near(result.side, std::sqrt(50.0 * 500.0), 1e-12,
              "fixed-density side must follow sqrt(50*N)");
}

void test_fixed_density_rejects_nonstandard_side() {
  const formation::ArenaResolution result =
      formation::resolve_arena_side(500, "fixed_density", "250");
  expect_true(!result.success,
              "statistical mode must reject a nonstandard arena side");
}

void test_qualitative_mode_accepts_explicit_side() {
  const formation::ArenaResolution result =
      formation::resolve_arena_side(500, "qualitative", "250");
  expect_true(result.success, "qualitative mode must accept an explicit side");
  expect_true(result.qualitative, "qualitative mode must be marked separately");
  expect_near(result.side, 250.0, 1e-12,
              "qualitative mode must preserve the requested side");
}

void test_qualitative_mode_requires_valid_explicit_side() {
  expect_true(!formation::resolve_arena_side(500, "qualitative", "").success,
              "qualitative mode must not silently use the statistical side");
  expect_true(!formation::resolve_arena_side(500, "qualitative", "nan").success,
              "qualitative mode must reject a non-finite side");
}

void test_unknown_mode_is_rejected() {
  expect_true(!formation::resolve_arena_side(500, "benchmark", "250").success,
              "unknown experiment modes must fail closed");
}

}  // namespace

int main() {
  test_fixed_density_uses_protocol_side_by_default();
  test_fixed_density_rejects_nonstandard_side();
  test_qualitative_mode_accepts_explicit_side();
  test_qualitative_mode_requires_valid_explicit_side();
  test_unknown_mode_is_rejected();
  std::cout << "experiment config tests passed" << std::endl;
  return EXIT_SUCCESS;
}
