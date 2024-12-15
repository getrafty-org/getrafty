package org.getrafty.fragments.services;

import com.intellij.openapi.components.Service;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.vfs.VirtualFile;
import org.jetbrains.annotations.NotNull;

import java.util.*;

@Service(Service.Level.PROJECT)
public final class FragmentsIndex {
    private final Map<String, Map<VirtualFile, List<OffsetRange>>> snippetIndex = new HashMap<>();

    public FragmentsIndex(@NotNull Project project) {
        // Any initialization logic if needed
    }

    public void registerSnippet(@NotNull String hash, @NotNull VirtualFile file, int startOffset, int endOffset) {
        snippetIndex.computeIfAbsent(hash, a -> new HashMap<>())
                .computeIfAbsent(file, a -> new ArrayList<>())
                .add(new OffsetRange(startOffset, endOffset));
    }

    public Map<String, Map<VirtualFile, List<OffsetRange>>> getAllSnippets() {
        return snippetIndex;
    }

    public List<OffsetRange> getOffsetsForFile(@NotNull String hash, @NotNull VirtualFile file) {
        return snippetIndex.getOrDefault(hash, Collections.emptyMap())
                .getOrDefault(file, Collections.emptyList());
    }

    public void clearSnippetsForFile(@NotNull VirtualFile file) {
        snippetIndex.values().forEach(fileOccurrences -> fileOccurrences.remove(file));
        snippetIndex.entrySet().removeIf(entry -> entry.getValue().isEmpty());
    }

    public void removeSnippet(@NotNull String hash, @NotNull VirtualFile file) {
        synchronized (snippetIndex) {
            snippetIndex.computeIfPresent(hash, (key, fileSet) -> {
                fileSet.remove(file);
                return fileSet.isEmpty() ? null : fileSet;
            });
        }
    }

    public record OffsetRange(int start, int end) {
    }
}
