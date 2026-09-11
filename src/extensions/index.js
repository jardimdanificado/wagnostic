/**
 * Piolho Extensions Index & Default Registry
 */

const { ExtensionRegistry } = require('./registry');
const { framebufferExtension, getFramebuffer } = require('./framebuffer');
const { clockExtension } = require('./clock');
const { keyboardExtension } = require('./keyboard');
const { mouseExtension } = require('./mouse');
const { gamepadExtension } = require('./gamepad');
const { loggerExtension } = require('./logger');
const { gifExtension } = require('./gif');

function createDefaultRegistry() {
  const registry = new ExtensionRegistry();
  registry.register(framebufferExtension);
  registry.register(clockExtension);
  registry.register(keyboardExtension);
  registry.register(mouseExtension);
  registry.register(gamepadExtension);
  registry.register(loggerExtension);
  registry.register(gifExtension);
  return registry;
}

const defaultRegistry = createDefaultRegistry();

module.exports = {
  ExtensionRegistry,
  createDefaultRegistry,
  defaultRegistry,
  framebufferExtension,
  getFramebuffer,
  clockExtension,
  keyboardExtension,
  mouseExtension,
  gamepadExtension,
  loggerExtension,
  gifExtension
};
