# Introduction

## Purpose

This section should describe the purpose of the document. It should identify the readers. It should explain which product's software requirements and design this requirements document describes.

## Scope

This section should describe what the document includes and excludes.

# Overall Overview

This section describes general factors that affect the product and product requirements. It consists of the following 4 parts. One point to note is that this section does not describe specific requirements, but rather makes the specific requirements to be described easier to understand.

## Software Overview

### Project Introduction

Describe the background of the project described by this software requirement. For example: Is this project one in a series of versions, or does it replace an existing system, or is it a new independent project.

### Product Environment Introduction

Describe the overall environment composed of this product and other products or projects.

1. If this product is independent and completely self-contained, state this here.

2. If the product defined by SRS is a component of a larger system or project (this situation often occurs), then:

> A. Describe the function of each component of this larger system or project, and identify the interfaces.
>
> B. Identify the main external interfaces of this software product. (Note: Detailed descriptions of these interfaces are not provided in this section; they are provided in other parts of the SRS.)
>
> C. Describe the relevant product hardware and external devices used. (Note: This is only an overview description.)

It is very helpful to describe the main components of the larger system or project, their interconnectivity, and external interfaces using block diagrams. This section should not propose a specific design solution or specific design constraints on the solution (specific design constraints will be described in the specific requirements section). The content of this section is the basis for generating design constraints.


## Software Functions

Overview of the main functions that the software must implement and those implemented through user operations. Only a brief description is needed here (e.g., a table of contents), with detailed descriptions in the detailed requirements section. Organize requirement functions so that readers can understand them and this can guide subsequent design and testing. Charts can be used to represent the relationships between major requirement groups, such as high-level data flow diagrams, object-oriented analysis, etc.

Sometimes the functional overview required in this section can be directly quoted from higher-level specifications (if they exist) that assign specific functions to this software product.

This section should not describe specific requirements. However, the content of this section is the basis for the specific requirements section.

## Design Constraints

Describe matters that may limit developers' choices, and what capabilities the features do not support need to be described

## Assumptions and Dependencies

*Describe clearly the dependencies of this iteration*

# Feature 1 Requirements Analysis & Design

## Overall Introduction

## Functional Requirements

### Functional Requirement 1

> Use short words as the functional requirement name, do not use "Functional Requirement (1)" as the functional name
>

1.  Introduction

> List the functional requirements related to this feature item by item. Include how the project responds to expected error inputs, illegal conditions, and invalid inputs. Requirements should be concise, complete, unambiguous, verifiable, and necessary. Use "TBD" when needed information is uncertain.

2.  Input

> This subsection should contain the following:
>
> A. Detailed description of all input data for this function, including:
>
> Input source
>
> Quantity
>
> Unit of measurement
>
> Time requirements
>
> Valid input range including precision and tolerance
>
> B. References to interface specifications or interface control documents where appropriate.

3.  Processing

> This subsection should describe all operations performed on input data and how output is obtained. This includes the following specifications:
>
> A. Validity detection of input data.
>
> B. Exact sequence of operations, including timing of events.
>
> C. Responses to exceptional conditions, such as:
>
> Overflow
>
> Communication failure
> Communication failure
>
> Error handling
>
> D. Any method used to convert system inputs to corresponding outputs (such as equations, mathematical algorithms, logical operations). For example, this may describe the following aspects:
>
> Calculation formula for withholding income tax in payroll.
>
> Meteorological model used for weather forecasting.
>
> E. Validity detection for output data.

4.  Output

> This subparagraph should contain:
>
> A. Detailed description of all output data for this function, this description includes:
>
> Output destination (e.g. printer, file)
>
> Quantity
>
> Measurement unit
>
> Timing
>
> Valid output range including precision and tolerance
>
> Handling of illegal values
>
> Error messages
>
> B. Where appropriate, provide references to interface specifications or interface control documents.

Additionally, for systems where requirements concentrate on input/output behavior, SRS should describe all important input/output behaviors and sequence of input/output pairs. For systems that need to remember their behavior to react based on inputs and past behaviors, sequence of input/output pairs is required; this functional behavior is similar to finite state machine.

### Functional Requirement 2

## Non-functional Requirements

### Maintainability

### Testability

### Portability

### Reliability

### Platformization Requirements

GE is a onetrack component, nowhere allows distinguishing chips, but some features involve E2E delivery, such as underlying interface calls like RTS, need to confirm clearly RTS interface support capabilities under each platform

### Feature Cross Analysis

See [cross_feature_check.md](cross_feature_check.md)

## Performance

### Model Compilation Duration

*All code involved in compilation needs to evaluate performance impact of new code (currently many customers no longer agree to count duration from second iteration start, so every stage of end-to-end is important), identified key stages: optimizeStage1, optimizeStage2, build, loadmodelonline four stages. Code involving above four stages, 3.4.1 must fill impact assessment.*

### OM Size and Load Memory Usage

*Load memory usage needs to consider memory-sensitive scenarios, new features原则上 should not cause memory increase (actually also belongs to compatibility issue), if new features need extra memory,原则上 default off*

### Execution Performance

*All execution-involved code needs to evaluate performance impact of new code, impact needs to be evaluated to ns level (small to string operations, profiling etc.), graph mode dynamic shape GE directly participates in each operator scheduling dispatch, single operator both fixed and dynamic shape involved, will affect final execution duration*

*Fixed shape scenarios mainly involve stream allocation scenarios etc., need to evaluate whether affects concurrency*

## Interface Design

### New/Modified Interface Description

*Involving new/modified interfaces needs to be maintained in 0-layer design document, here can directly reference.*

### Interface Checklist

-------------- ---------------------------------------------------------------------------- --------------
| **Check Item**   | **Sub-check Item**                                                 | **Involved** |
| ------------ | ------------------------------------------------------------ | :----------- |
| **Interface Description** | Whether interface needs review, review needs to focus on interface compatibility and interface constraints (optional between modules, mandatory between components) |              |
| **Interface Description** | Whether needs supplementary documentation explanation                                         |              |
| **Interface Description** | Whether interface document clearly explains interface prototype, functionality, return value etc.             |              |
| **Interface Compatibility** | Interface behavior compatibility, whether behavior changes before and after modification, whether needs to notify related components |              |
| **Interface Compatibility** | Whether interface has compatibility, whether new interface can work normally on old version, involves this component and external component new interfaces |              |
| **Interface Compatibility** | Whether interface involves usage scenarios, calling sequence etc. constraints                         |              |
| **Interface Constraints** | Whether can clearly report error when interface call doesn't meet constraint conditions                       |              |
| **Interface Constraints** | Whether needs to design separate test cases                                   |              |

-------------- ---------------------------------------------------------------------------- --------------



## Software Design

### Key Data Structures

### Key Technologies/Algorithms

### Process Design

### Modifications to Sub-modules

*If involves sub-module main process modification, needs to notify MDE to update module-level design document*

### Error Handling

#### System Errors

*Describe how errors like memory allocation failure, task creation failure etc. are handled.*

#### Interface Errors

*Describe error codes to be generated and used by external entities*

## Security Check

### Coding Military Rules

Refer to coding_military_rules.md

### Coding Checklist

| **Check Item**                                         | **Check Item Description**                                               | **Involved** |
| -------------------------------------------------- | ------------------------------------------------------------ | ------------ |
| **Whether involves resource lifecycle management (e.g. thread pool, memory pool etc.)** | Resource lifecycle too long may lead to resource shortage, slow response, system crash etc. serious system stability and reliability issues. Clearly defining resource lifecycle (process-level/Session-level/Graph-level etc.) can help us better control resource usage, ensure effective utilization and timely release of resources. For using long-lifecycle resources, need code design meeting review or consider other technical means. |              |
| **Whether creating new thread**                                 | 1) Newly created threads need to copy ThreadLocalContext, ErrorContext etc. context information. Performance scenarios as special case separate analysis, if not needed can not copy.  2) Newly created threads need to set thread name. |              |

## Compatibility Check

Key focus whether old om can execute under new version, new om executing under old version currently not mandatory requirement, but best ensure

## DT Design

### Test Boundary

*Test entry (based on what interface to test), test exit (what data validation to do), stub modules*

### Test Design

*Which categories of cases, export to case list, table only provides a few categories, can add*

**Case types: UT, ST, BBIT**



| **Test Category** | **Key Test Items** | **Test Method** | **Case Type** |
| ------------ | -------------- | ------------ | ------------ |
| **Functionality**     |                |              |              |
| **Performance**     |                |              |              |
| **Precision**     |                |              |              |
| **Compatibility**   |                |              |              |
| **Feature Cross** |                |              |              |


-------------- ---------------- --------------------------------------- --------------

### Test Framework Design

> *Whether current test framework meets test requirements, if not, add corresponding class design*

## Acceptance Criteria
