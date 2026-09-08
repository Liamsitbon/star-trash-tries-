using UnityEngine;

namespace VivifyTimelinePreview
{

[ExecuteInEditMode]
[RequireComponent(typeof(Camera))]
public class BeatSaberSpectatorCamera : MonoBehaviour
{
    [Header("Free camera")]
    public bool enableFreeFly = true;
    public float moveSpeed = 8f;
    public float fastMultiplier = 3f;
    public float lookSensitivity = 2f;
    public bool requireRightMouseButton = true;

    [Header("Orbit mode")]
    public bool orbitMode = false;
    public Transform orbitTarget;
    public float orbitDistance = 14f;
    public float orbitHeight = 4f;
    public float orbitSpeed = 18f;

    private float yaw;
    private float pitch;

    private void OnEnable()
    {
        Vector3 e = transform.eulerAngles;
        yaw = e.y;
        pitch = e.x;
    }

    private void Update()
    {
        if (!Application.isPlaying) return;

        if (orbitMode && orbitTarget != null)
        {
            yaw += Input.GetAxis("Horizontal") * orbitSpeed * Time.unscaledDeltaTime;
            Quaternion rot = Quaternion.Euler(15f, yaw, 0f);
            Vector3 offset = rot * new Vector3(0f, orbitHeight, -Mathf.Max(0.5f, orbitDistance));
            transform.position = orbitTarget.position + offset;
            transform.LookAt(orbitTarget.position + Vector3.up * 1.2f);
            return;
        }

        if (!enableFreeFly) return;
        bool looking = !requireRightMouseButton || Input.GetMouseButton(1);
        if (looking)
        {
            yaw += Input.GetAxis("Mouse X") * lookSensitivity;
            pitch -= Input.GetAxis("Mouse Y") * lookSensitivity;
            pitch = Mathf.Clamp(pitch, -89f, 89f);
            transform.rotation = Quaternion.Euler(pitch, yaw, 0f);
        }

        float speed = moveSpeed * (Input.GetKey(KeyCode.LeftShift) || Input.GetKey(KeyCode.RightShift) ? fastMultiplier : 1f);
        Vector3 move = Vector3.zero;
        if (Input.GetKey(KeyCode.W)) move += Vector3.forward;
        if (Input.GetKey(KeyCode.S)) move += Vector3.back;
        if (Input.GetKey(KeyCode.A)) move += Vector3.left;
        if (Input.GetKey(KeyCode.D)) move += Vector3.right;
        if (Input.GetKey(KeyCode.E)) move += Vector3.up;
        if (Input.GetKey(KeyCode.Q)) move += Vector3.down;
        if (move.sqrMagnitude > 1f) move.Normalize();
        transform.position += transform.TransformDirection(move) * speed * Time.unscaledDeltaTime;
    }
}
}
