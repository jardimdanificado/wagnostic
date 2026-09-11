/**
 * comm:workers Extension
 * 
 * Reports worker availability in the host coordinator.
 * Returns 1 (available) directly.
 */

const commWorkersExtension = {
  name: ['comm:workers', 'workers'],

  onRequest(worker, host) {
    return (host.workers !== undefined) ? 1 : 0;
  }
};

module.exports = { commWorkersExtension };
