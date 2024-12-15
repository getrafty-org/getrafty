package org.getrafty.snippy.status;

import com.intellij.openapi.project.Project;
import com.intellij.openapi.wm.StatusBarWidget;
import com.intellij.util.Consumer;
import org.getrafty.snippy.services.SnippetService;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.awt.*;
import java.awt.event.MouseEvent;

public class ModeStatusWidget implements StatusBarWidget, StatusBarWidget.TextPresentation {
    private final SnippetService snippetService;

    public ModeStatusWidget(@NotNull Project project) {
        this.snippetService = project.getService(SnippetService.class);
    }

    @NotNull
    @Override
    public String ID() {
        return "SnippetModeWidget";
    }

    @Nullable
    @Override
    public WidgetPresentation getPresentation() {
        return this; // Return itself as the TextPresentation
    }

    @NotNull
    @Override
    public String getText() {
        return "Snippet Focus: " + (snippetService.isMaintainerMode() ? "Maintainer" : "User");
    }


    @Nullable
    @Override
    public String getTooltipText() {
        return "Snippet Focus: " + (snippetService.isMaintainerMode() ? "Maintainer" : "User");
    }


    @Override
    public float getAlignment() {
        return Component.CENTER_ALIGNMENT;
    }
}
