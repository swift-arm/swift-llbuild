//===- BuildEngine.h --------------------------------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef LLBUILD_CORE_BUILDENGINE_H
#define LLBUILD_CORE_BUILDENGINE_H

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace llbuild {
namespace core {

// FIXME: Need to abstract KeyType;
typedef std::string KeyType;
// FIXME: Need to abstract ValueType;
typedef int ValueType;

class BuildDB;
class BuildEngine;

/// This object contains the result of executing a task to produce the value for
/// a key.
struct Result {
  /// The last value that resulted from executing the task.
  ValueType Value = {};

  /// The build timestamp during which the result \see Value was computed.
  uint64_t ComputedAt = 0;

  /// The build timestamp at which this result was last checked to be
  /// up-to-date.
  ///
  /// \invariant BuiltAt >= ComputedAt
  //
  // FIXME: Think about this representation more. The problem with storing this
  // field here in this fashion is that every build will result in bringing all
  // of the BuiltAt fields up to date. That is unfortunate from a persistence
  // perspective, where it would be ideal if we didn't touch any disk state for
  // null builds.
  uint64_t BuiltAt = 0;

  /// The explicit dependencies required by the generation.
  //
  // FIXME: At some point, figure out the optimal representation for this field,
  // which is likely to be a lot of the resident memory size.
  std::vector<KeyType> Dependencies;
};

/// A task object represents an abstract in progress computation in the build
/// engine.
///
/// The task represents not just the primary computation, but also the process
/// of starting the computation and necessary input dependencies. Tasks are
/// expected to be created in response to \see BuildEngine requests to initiate
/// the production of particular result value.
///
/// The creator may use \see BuildEngine::taskNeedsInput() to specify input
/// dependencies on the Task. The Task itself may also specify additional input
/// dependencies dynamically during the execution of \see Task::start() or \see
/// Task::provideValue().
///
/// Once a task has been created and registered, the BuildEngine will invoke
/// \see Task::start() to initiate the computation. The BuildEngine will provide
/// the in progress task with its requested inputs via \see
/// Task::provideValue().
///
/// After all inputs requested by the Task have been delivered, the BuildEngine
/// will invoke \see Task::finish() to instruct the Task to complete its
/// computation and provide the output.
//
// FIXME: Define parallel execution semantics.
class Task {
public:
  /// The name of the task, for debugging purposes.
  //
  // FIXME: Eliminate this?
  std::string Name;

public:
  Task(const std::string& Name) : Name(Name) {}

  virtual ~Task();

  /// Executed by the build engine when the task should be started.
  virtual void start(BuildEngine&) = 0;

  /// Invoked by the build engine to provide an input value as it becomes
  /// available.
  ///
  /// \param InputID The unique identifier provided to the build engine to
  /// represent this input when requested in \see
  /// BuildEngine::taskNeedsInput().
  ///
  /// \param Value The computed value for the given input.
  virtual void provideValue(BuildEngine&, uintptr_t InputID,
                             ValueType Value) = 0;

  /// Executed by the build engine to retrieve the task output, after all inputs
  /// have been provided.
  //
  // FIXME: Is it ever useful to provide the build engine here? It would be more
  // symmetric.
  virtual ValueType finish() = 0;
};

class Rule {
public:
  KeyType Key;
  std::function<Task*(BuildEngine&)> Action;
  std::function<bool(const Rule&, const ValueType)> IsResultValid;
};

class BuildEngine {
  void *Impl;

public:
  /// Create a build engine using a particular database delegate.
  explicit BuildEngine();
  ~BuildEngine();

  /// @name Rule Definition
  /// @{

  /// Add a rule which the engine can use to produce outputs.
  void addRule(Rule &&Rule);

  /// @}

  /// @name Client API
  /// @{

  /// Build the result for a particular key.
  ValueType build(KeyType Key);

  /// Attach a database for persisting build state.
  ///
  /// A database should only be attached immediately after creating the engine,
  /// it is an error to attach a database after adding rules or initiating any
  /// builds, or to attempt to attach multiple databases.
  void attachDB(std::unique_ptr<BuildDB> Database);

  /// Enable tracing into the given output file.
  ///
  /// \returns True on success.
  bool enableTracing(const std::string& Path, std::string* Error_Out);

  /// @}

  /// @name Task Management APIs
  /// @{

  /// Register the given task, in response to a Rule evaluation.
  ///
  /// The engine tasks ownership of the \arg Task, and it is expected to
  /// subsequently be returned as the task to execute for a Rule evaluation.
  ///
  /// \returns The provided task, for the convenience of the client.
  Task* registerTask(Task* Task);

  /// Specify the given \arg Task depends upon the result of computing \arg Key.
  ///
  /// The result, when available, will be provided to the task via \see
  /// Task::provideValue(), supplying the provided \arg InputID to allow the
  /// task to identify the particular input.
  void taskNeedsInput(Task* Task, KeyType Key, uintptr_t InputID);
  
  /// @}
};

}
}

#endif
