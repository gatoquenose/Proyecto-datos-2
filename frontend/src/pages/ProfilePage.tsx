import { useState } from "react";
import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { usersClient } from "../api/usersClient";
import { ApiError, friendlyErrorMessage } from "../types/api";
import { useAuth } from "../auth/AuthContext";
import { ConfirmDialog } from "../components/ConfirmDialog";
import { ErrorState } from "../components/ErrorState";
import { LoadingState } from "../components/LoadingState";
import { QuotaBar } from "../components/QuotaBar";
import { validateEmail } from "../utils/validation";

export function ProfilePage(): JSX.Element {
  const { user, logout } = useAuth();
  const queryClient = useQueryClient();
  const [email, setEmail] = useState(user?.email ?? "");
  const [emailError, setEmailError] = useState("");
  const [message, setMessage] = useState("");
  const [showDelete, setShowDelete] = useState(false);
  const [confirmation, setConfirmation] = useState("");
  const quota = useQuery({ queryKey: ["quota"], queryFn: usersClient.getMyQuota, enabled: Boolean(user) });
  const update = useMutation({
    mutationFn: (nextEmail: string) => usersClient.updateUser(user?.id ?? "", { email: nextEmail }),
    onSuccess: (nextUser) => { queryClient.setQueryData(["currentUser"], nextUser); setMessage("Correo actualizado."); },
    onError: (error: unknown) => setEmailError(error instanceof ApiError ? friendlyErrorMessage(error.body.code) : "No se pudo actualizar el correo."),
  });
  const remove = useMutation({ mutationFn: () => usersClient.deleteUser(user?.id ?? ""), onSuccess: logout, onError: (error: unknown) => setMessage(error instanceof ApiError ? `${friendlyErrorMessage(error.body.code)} (${error.body.traceId})` : "No se pudo eliminar la cuenta.") });
  if (!user) return <LoadingState />;
  if (quota.isPending) return <LoadingState label="Cargando perfil…" />;
  if (quota.isError) return <ErrorState error={quota.error} retry={() => void quota.refetch()} />;
  const saveEmail = (): void => {
    const error = validateEmail(email);
    setEmailError(error ?? "");
    if (!error && email !== user.email) update.mutate(email);
  };
  return <section className="profile-page"><h1>Mi perfil</h1>{message && <p className="success-box" role="status">{message}</p>}
    <div className="card profile-card"><p><strong>Usuario</strong><br />{user.username}<small>El username es inmutable.</small></p>
      <label>Correo<input type="email" value={email} onChange={(event) => setEmail(event.target.value)} aria-invalid={Boolean(emailError)} aria-describedby={emailError ? "profile-email-error" : undefined} autoComplete="email" />{emailError && <small id="profile-email-error" className="field-error">{emailError}</small>}</label>
      <button type="button" className="button primary" onClick={saveEmail} disabled={update.isPending}>Guardar correo</button>
    </div>
    <div className="card"><h2>Almacenamiento</h2><QuotaBar quotaBytes={quota.data.quotaBytes} usedBytes={quota.data.usedBytes} /></div>
    <div className="card danger-zone"><h2>Eliminar cuenta</h2><p>Esta acción no se puede deshacer.</p><button type="button" className="button danger" onClick={() => setShowDelete(true)}>Eliminar mi cuenta</button></div>
    {showDelete && <ConfirmDialog title="Confirmar eliminación de cuenta" confirmLabel="Eliminar cuenta" confirmDisabled={confirmation !== user.username} onCancel={() => { setShowDelete(false); setConfirmation(""); }} onConfirm={() => { if (confirmation === user.username) remove.mutate(); }}>
      <p>Escribe <strong>{user.username}</strong> para confirmar.</p><input aria-label="Confirmar username" value={confirmation} onChange={(event) => setConfirmation(event.target.value)} autoComplete="off" />
      {confirmation && confirmation !== user.username && <small className="field-error">El username no coincide.</small>}
    </ConfirmDialog>}
  </section>;
}
