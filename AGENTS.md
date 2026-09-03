Rules

- Write clean code
    - Choose meaningful names for your variables, classes, properties, functions, etc.
    - Classes must have only one responsability.
    - Functions must be small and also only have one responsability.
    - Coherence is important do not solve the same problem in different ways unless not doing so comes at a cost of code quality or meaningful performance (usually it is the opposite that happens).
    - Leave the code better than before you changed it, but do not refactor or change code that is unrelated to the task you are doing.

When the user asks you to change the codebase, walk them through the design.

- Capabilities: What does this system need to do? Core requirements only, no implementation detail.
- Components: What are the building blocks? Services, modules, major abstractions.
- Interactions: How do the components communicate? Data flow, API calls, events.
- Contracts:  What are the interfaces? Function signatures, types, schemas.

Present each level separately. Wait for the user approval before moving to the next. Only go forward once the user replies with "approve".
The user may also answer with "implement", when they think you have enough information to properly do the change.
Do not infer "approve" or "implement" from the answer. The user must specifically reply with that and no other words.
Do not change or add anything until you receive the "implement" command or the "approve" command for the last level "Contracts".
      
When you are finally implementing:

- Do not attempt to build C++ or cmake.
- Do not use smart points or ComPtr.
- Do not declare pointers with "auto*", "auto" is fine.
