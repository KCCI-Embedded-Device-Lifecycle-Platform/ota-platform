package ota.platform.server.hawkbit;

import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

import org.springframework.stereotype.Component;

@Component
public class HawkbitFirmwareActionTracker {

    private final ConcurrentMap<String, HawkbitFirmwareAction> activeActions = new ConcurrentHashMap<>();

    private final ConcurrentMap<String, Long> installRequestedActions = new ConcurrentHashMap<>();

    private final ConcurrentMap<String, Integer> lastFirmwareStates = new ConcurrentHashMap<>();

    private final ConcurrentMap<String, Integer> lastUpdateResults = new ConcurrentHashMap<>();

    public void track(HawkbitFirmwareAction action) {
        activeActions.put(action.endpoint(), action);
        installRequestedActions.remove(action.endpoint());
        lastFirmwareStates.remove(action.endpoint());
        lastUpdateResults.remove(action.endpoint());
    }

    public Optional<HawkbitFirmwareAction> find(
            String endpoint) {

        return Optional.ofNullable(
                activeActions.get(endpoint));
    }

    public boolean isTracking(
            String endpoint,
            long actionId) {

        HawkbitFirmwareAction action = activeActions.get(endpoint);

        return action != null
                && action.actionId() == actionId;
    }

    public boolean markInstallRequested(
            String endpoint,
            long actionId) {

        HawkbitFirmwareAction action = activeActions.get(endpoint);

        if (action == null || action.actionId() != actionId) {
            return false;
        }

        return installRequestedActions.putIfAbsent(
                endpoint,
                actionId) == null;
    }

    public boolean markFirmwareState(
            String endpoint,
            long actionId,
            int firmwareState) {

        HawkbitFirmwareAction action = activeActions.get(endpoint);

        if (action == null || action.actionId() != actionId) {
            return false;
        }

        Integer previous = lastFirmwareStates.put(
                endpoint,
                firmwareState);

        return previous == null
                || previous.intValue() != firmwareState;
    }

    public boolean markUpdateResult(
            String endpoint,
            long actionId,
            int updateResult) {

        HawkbitFirmwareAction action = activeActions.get(endpoint);

        if (action == null || action.actionId() != actionId) {
            return false;
        }

        Integer previous = lastUpdateResults.put(endpoint, updateResult);

        return previous == null || previous.intValue() != updateResult;
    }

    public void remove(String endpoint, long actionId) {
        activeActions.computeIfPresent(
                endpoint,
                (key, action) -> action.actionId() == actionId
                        ? null
                        : action);

        installRequestedActions.remove(endpoint, actionId);
        lastFirmwareStates.remove(endpoint);
        lastUpdateResults.remove(endpoint);
    }
}