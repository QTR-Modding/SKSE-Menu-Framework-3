## Agent Behavior 

- Write clean code
  - Choose meaningful names for your variables, classes, properties, functions, etc.
  - Classes should be small enough to remain focused and easy to understand, but they should not be made small just to satisfy an arbitrary size limit. Cohesion and clarity are more important than size.
  - Functions should be small enough to make their purpose and behavior easy to understand, but they should not be split when doing so makes the code harder to follow. Clarity is more important than line count.
  - Files should be small enough to remain easy to navigate and should contain closely related code, but they should not be split merely to reduce their size. Keeping related code together is more important than an arbitrary file size.
  - Coherence is important. Do not solve the same problem in different ways unless not doing so comes at a cost of code quality or meaningful performance (usually it is the opposite that happens).
  - Leave the code better than before you changed it, but do not refactor or change code that is unrelated to the task you are doing.
  - Handle errors deliberately. Do not silently ignore failures or hide invalid states.

When the user asks you to change the codebase, walk them through the design.

- Capabilities (Core requirements only, no implementation detail)
    - For a feature: what does this system need to do? 
    - For a fix, you must tell the user what you believe is causing the issue and what you want to do in order to fix it. 
- Components: What building blocks will be added or changed and how? Services, modules, major abstractions.
- Interactions: What will be added or changed to how the components communicate? Data flow, API calls, events.
- Contracts:  What interfaces will be added or changed? Function signatures, types, schemas.

Present each level separately. Wait for the user approval before moving to the next. Only go forward once the user replies with "approve".
The user may also answer with "implement", when they think you have enough information to properly do the change.
Do not infer "approve" or "implement" from the answer. The user must specifically reply with that and no other words.
Do not change or add anything until you receive the "implement" command or the "approve" command for the last level "Contracts".
      
When you are finally implementing:

- Do not attempt to build C++ or cmake.
- Do not use smart points or ComPtr.
- Do not declare pointers with "auto*", "auto" is fine.
- Do not rename or delete any function from include/SKSEMenuFramework.h unless specifically asked to do it
