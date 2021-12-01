// Copyright 2021 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0(the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_SEM_FUNCTION_H_
#define SRC_SEM_FUNCTION_H_

#include <array>
#include <utility>
#include <vector>

#include "src/ast/variable.h"
#include "src/sem/call.h"
#include "src/utils/unique_vector.h"

namespace tint {

// Forward declarations
namespace ast {
class BuiltinDecoration;
class Function;
class LocationDecoration;
class ReturnStatement;
}  // namespace ast

namespace sem {

class Intrinsic;
class Variable;

/// WorkgroupDimension describes the size of a single dimension of an entry
/// point's workgroup size.
struct WorkgroupDimension {
  /// The size of this dimension.
  uint32_t value;
  /// A pipeline-overridable constant that overrides the size, or nullptr if
  /// this dimension is not overridable.
  const ast::Variable* overridable_const = nullptr;
};

/// WorkgroupSize is a three-dimensional array of WorkgroupDimensions.
using WorkgroupSize = std::array<WorkgroupDimension, 3>;

/// Function holds the semantic information for function nodes.
class Function : public Castable<Function, CallTarget> {
 public:
  /// A vector of [Variable*, ast::VariableBindingPoint] pairs
  using VariableBindings =
      std::vector<std::pair<const Variable*, ast::VariableBindingPoint>>;

  /// Constructor
  /// @param declaration the ast::Function
  /// @param return_type the return type of the function
  /// @param parameters the parameters to the function
  Function(const ast::Function* declaration,
           Type* return_type,
           std::vector<Parameter*> parameters);

  /// Destructor
  ~Function() override;

  /// @returns the ast::Function declaration
  const ast::Function* Declaration() const { return declaration_; }

  /// @returns the workgroup size {x, y, z} for the function.
  const sem::WorkgroupSize& WorkgroupSize() const { return workgroup_size_; }

  /// Sets the workgroup size {x, y, z} for the function.
  /// @param workgroup_size the new workgroup size of the function
  void SetWorkgroupSize(sem::WorkgroupSize workgroup_size) {
    workgroup_size_ = std::move(workgroup_size);
  }

  /// @returns all directly referenced global variables
  const utils::UniqueVector<const GlobalVariable*>& DirectlyReferencedGlobals()
      const {
    return directly_referenced_globals_;
  }

  /// Records that this function directly references the given global variable.
  /// Note: Implicitly adds this global to the transtively-called globals.
  /// @param global the module-scope variable
  void AddDirectlyReferencedGlobal(const sem::GlobalVariable* global) {
    directly_referenced_globals_.add(global);
    transitively_referenced_globals_.add(global);
  }

  /// @returns all transitively referenced global variables
  const utils::UniqueVector<const GlobalVariable*>&
  TransitivelyReferencedGlobals() const {
    return transitively_referenced_globals_;
  }

  /// Records that this function transitively references the given global
  /// variable.
  /// @param global the module-scoped variable
  void AddTransitivelyReferencedGlobal(const sem::GlobalVariable* global) {
    transitively_referenced_globals_.add(global);
  }

  /// @returns the list of functions that this function transitively calls.
  const utils::UniqueVector<const Function*>& TransitivelyCalledFunctions()
      const {
    return transitively_called_functions_;
  }

  /// Records that this function transitively calls `function`.
  /// @param function the function this function transitively calls
  void AddTransitivelyCalledFunction(const Function* function) {
    transitively_called_functions_.add(function);
  }

  /// @returns the list of intrinsics that this function directly calls.
  const utils::UniqueVector<const Intrinsic*>& DirectlyCalledIntrinsics()
      const {
    return directly_called_intrinsics_;
  }

  /// Records that this function transitively calls `intrinsic`.
  /// @param intrinsic the intrinsic this function directly calls
  void AddDirectlyCalledIntrinsic(const Intrinsic* intrinsic) {
    directly_called_intrinsics_.add(intrinsic);
  }

  /// @returns the list of direct calls to functions / intrinsics made by this
  /// function
  std::vector<const Call*> DirectCallStatements() const {
    return direct_calls_;
  }

  /// Adds a record of the direct function / intrinsic calls made by this
  /// function
  /// @param call the call
  void AddDirectCall(const Call* call) { direct_calls_.emplace_back(call); }

  /// @param target the target of a call
  /// @returns the Call to the given CallTarget, or nullptr the target was not
  /// called by this function.
  const Call* FindDirectCallTo(const CallTarget* target) const {
    for (auto* call : direct_calls_) {
      if (call->Target() == target) {
        return call;
      }
    }
    return nullptr;
  }

  /// @returns the list of callsites of this function
  std::vector<const Call*> CallSites() const { return callsites_; }

  /// Adds a record of a callsite to this function
  /// @param call the callsite
  void AddCallSite(const Call* call) { callsites_.emplace_back(call); }

  /// @returns the ancestor entry points
  const std::vector<const Function*>& AncestorEntryPoints() const {
    return ancestor_entry_points_;
  }

  /// Adds a record that the given entry point transitively calls this function
  /// @param entry_point the entry point that transtively calls this function
  void AddAncestorEntryPoint(const sem::Function* entry_point) {
    ancestor_entry_points_.emplace_back(entry_point);
  }

  /// Retrieves any referenced location variables
  /// @returns the <variable, decoration> pair.
  std::vector<std::pair<const Variable*, const ast::LocationDecoration*>>
  TransitivelyReferencedLocationVariables() const;

  /// Retrieves any referenced builtin variables
  /// @returns the <variable, decoration> pair.
  std::vector<std::pair<const Variable*, const ast::BuiltinDecoration*>>
  TransitivelyReferencedBuiltinVariables() const;

  /// Retrieves any referenced uniform variables. Note, the variables must be
  /// decorated with both binding and group decorations.
  /// @returns the referenced uniforms
  VariableBindings TransitivelyReferencedUniformVariables() const;

  /// Retrieves any referenced storagebuffer variables. Note, the variables
  /// must be decorated with both binding and group decorations.
  /// @returns the referenced storagebuffers
  VariableBindings TransitivelyReferencedStorageBufferVariables() const;

  /// Retrieves any referenced regular Sampler variables. Note, the
  /// variables must be decorated with both binding and group decorations.
  /// @returns the referenced storagebuffers
  VariableBindings TransitivelyReferencedSamplerVariables() const;

  /// Retrieves any referenced comparison Sampler variables. Note, the
  /// variables must be decorated with both binding and group decorations.
  /// @returns the referenced storagebuffers
  VariableBindings TransitivelyReferencedComparisonSamplerVariables() const;

  /// Retrieves any referenced sampled textures variables. Note, the
  /// variables must be decorated with both binding and group decorations.
  /// @returns the referenced sampled textures
  VariableBindings TransitivelyReferencedSampledTextureVariables() const;

  /// Retrieves any referenced multisampled textures variables. Note, the
  /// variables must be decorated with both binding and group decorations.
  /// @returns the referenced sampled textures
  VariableBindings TransitivelyReferencedMultisampledTextureVariables() const;

  /// Retrieves any referenced variables of the given type. Note, the variables
  /// must be decorated with both binding and group decorations.
  /// @param type_info the type of the variables to find
  /// @returns the referenced variables
  VariableBindings TransitivelyReferencedVariablesOfType(
      const tint::TypeInfo& type_info) const;

  /// Retrieves any referenced variables of the given type. Note, the variables
  /// must be decorated with both binding and group decorations.
  /// @returns the referenced variables
  template <typename T>
  VariableBindings TransitivelyReferencedVariablesOfType() const {
    return TransitivelyReferencedVariablesOfType(TypeInfo::Of<T>());
  }

  /// Checks if the given entry point is an ancestor
  /// @param sym the entry point symbol
  /// @returns true if `sym` is an ancestor entry point of this function
  bool HasAncestorEntryPoint(Symbol sym) const;

  /// Sets that this function has a discard statement
  void SetHasDiscard() { has_discard_ = true; }

  /// Returns true if this function has a discard statement
  /// @returns true if this function has a discard statement
  bool HasDiscard() const { return has_discard_; }

 private:
  VariableBindings TransitivelyReferencedSamplerVariablesImpl(
      ast::SamplerKind kind) const;
  VariableBindings TransitivelyReferencedSampledTextureVariablesImpl(
      bool multisampled) const;

  const ast::Function* const declaration_;

  sem::WorkgroupSize workgroup_size_;
  utils::UniqueVector<const GlobalVariable*> directly_referenced_globals_;
  utils::UniqueVector<const GlobalVariable*> transitively_referenced_globals_;
  utils::UniqueVector<const Function*> transitively_called_functions_;
  utils::UniqueVector<const Intrinsic*> directly_called_intrinsics_;
  std::vector<const Call*> direct_calls_;
  std::vector<const Call*> callsites_;
  std::vector<const Function*> ancestor_entry_points_;
  bool has_discard_ = false;
};

}  // namespace sem
}  // namespace tint

#endif  // SRC_SEM_FUNCTION_H_
