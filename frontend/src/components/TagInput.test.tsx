import { fireEvent, render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { TagInput } from "./TagInput";

describe("TagInput", () => {
  it("añade etiquetas con Enter y coma y permite quitarlas con un botón accesible", () => {
    const onChange = vi.fn();
    render(<TagInput value={[]} onChange={onChange} />);
    const input = screen.getByLabelText("Etiquetas");
    fireEvent.change(input, { target: { value: "algoritmos" } });
    fireEvent.keyDown(input, { key: "Enter" });
    fireEvent.change(input, { target: { value: "grafos" } });
    fireEvent.keyDown(input, { key: "," });
    expect(onChange).toHaveBeenNthCalledWith(1, ["algoritmos"]);
    expect(onChange).toHaveBeenNthCalledWith(2, ["grafos"]);
    render(<TagInput value={["algoritmos"]} onChange={onChange} />);
    expect(screen.getByRole("button", { name: "Quitar etiqueta algoritmos" })).toBeInTheDocument();
  });
});
