import { Navigate, Route, Routes } from "react-router-dom";
import { useAuth } from "../auth/AuthContext";
import { AppLayout } from "../components/AppLayout";
import { ProtectedRoute } from "../components/ProtectedRoute";
import { LoginPage, RegisterPage } from "../pages/LoginPage";
import { VideoListPage } from "../pages/VideoListPage";
import { NotFoundPage, ProfilePage, UploadPage, VideoDetailPage } from "../pages/PlaceholderPages";

function HomeRedirect(): JSX.Element {
  const { token } = useAuth();
  return <Navigate to={token ? "/videos" : "/login"} replace />;
}

export function App(): JSX.Element {
  return <Routes>
    <Route path="/login" element={<LoginPage />} />
    <Route path="/register" element={<RegisterPage />} />
    <Route element={<ProtectedRoute />}><Route element={<AppLayout />}>
      <Route path="/videos" element={<VideoListPage />} />
      <Route path="/videos/mine" element={<VideoListPage />} />
      <Route path="/videos/new" element={<UploadPage />} />
      <Route path="/videos/:id" element={<VideoDetailPage />} />
      <Route path="/videos/:id/edit" element={<VideoDetailPage />} />
      <Route path="/profile" element={<ProfilePage />} />
    </Route></Route>
    <Route path="/" element={<HomeRedirect />} />
    <Route path="*" element={<NotFoundPage />} />
  </Routes>;
}
