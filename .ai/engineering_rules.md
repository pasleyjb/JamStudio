# Engineering Rules

## General

- Use modern C++20.
- Use RAII.
- Prefer smart pointers.
- Avoid global variables.
- Favor composition over inheritance.
- Write readable code.
- Keep functions focused.

## Architecture

- Separate UI from business logic.
- Separate audio processing from rendering.
- Keep modules loosely coupled.
- Use interfaces where appropriate.
- Avoid circular dependencies.

## Documentation

Every public class should have documentation.

Complex algorithms should include comments explaining why they exist.

## Testing

Every major subsystem should eventually include unit tests.

## Performance

Optimize only after correctness.

Never sacrifice maintainability for micro-optimizations.