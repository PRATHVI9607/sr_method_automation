% Vibration Monitor GUI (Standalone .m File Version)
function VibrationMonitorApp()
    % ThingSpeak Setup
    channelID = 2956610;
    readAPIKey = '2BNC7Z193IYWD6C9';

    % Create UIFigure
    fig = uifigure('Name', 'Vibration Anomaly Monitor', 'Position', [100 100 1000 600]);

    % Dropdown for Date Range
    uidropdown(fig, 'Items', {'Last 24 Hours', 'Last 7 Days', 'This Month', 'All Available'}, ...
        'Position', [40 540 180 30], 'ValueChangedFcn', @(dd, event) updatePlots());

    % Refresh Button
    uibutton(fig, 'Position', [240 540 100 30], 'Text', 'Refresh', 'ButtonPushedFcn', @(btn, event) updatePlots());

    % Export Button
    uibutton(fig, 'Position', [360 540 100 30], 'Text', 'Export', 'ButtonPushedFcn', @(btn, event) exportPlots());

    % Threshold Slider
    uilabel(fig, 'Position', [490 540 90 30], 'Text', 'Threshold');
    thresholdSlider = uislider(fig, 'Position', [580 555 200 3], 'Limits', [0 100], 'Value', 50);

    % UIAxes for Similarity
    ax1 = uiaxes(fig, 'Position', [50 300 800 200]);
    title(ax1, 'Similarity Over Time');
    xlabel(ax1, 'Time');
    ylabel(ax1, 'Similarity (%)');

    % UIAxes for Anomaly Count
    ax2 = uiaxes(fig, 'Position', [50 50 800 200]);
    title(ax2, 'Daily Anomaly Count');
    xlabel(ax2, 'Date');
    ylabel(ax2, 'Count');

    % Internal state
    dataTable = [];  % Store fetched data

    % Update plots
    function updatePlots()
        dropdown = findobj(fig, 'Type', 'uidropdown');
        selected = dropdown.Value;
        nowTime = datetime('now');
        switch selected
            case 'Last 24 Hours'
                startTime = nowTime - hours(24);
            case 'Last 7 Days'
                startTime = nowTime - days(7);
            case 'This Month'
                startTime = dateshift(nowTime, 'start', 'month');
            otherwise
                startTime = nowTime - days(30);
        end

        try
            data = thingSpeakRead(channelID, ...
    'Fields', [1,2], ...
    'NumPoints', 8000, ...
    'ReadKey', readAPIKey, ...
    'OutputFormat', 'table');

% Now filter manually by date
data = data(data.Timestamps >= startTime & data.Timestamps <= datetime('now'), :);

            if isempty(data)
                uialert(fig, 'No data available.', 'Alert');
                return;
            end
            dataTable = data;
            vibration = data.vibration;
            similarity = data.similarity;
            timeStamps = data.Timestamps;

            cla(ax1); cla(ax2);
            hold(ax1, 'on');
            plot(ax1, timeStamps(vibration==0), similarity(vibration==0), 'go', 'DisplayName', 'Nominal');
            plot(ax1, timeStamps(vibration==1), similarity(vibration==1), 'ro', 'DisplayName', 'Anomaly');
            yline(ax1, thresholdSlider.Value, '--k', 'Threshold');
            legend(ax1, 'Location', 'best');
            grid(ax1, 'on');

            anomalyTimes = timeStamps(vibration==1);
            if ~isempty(anomalyTimes)
                d = dateshift(anomalyTimes, 'start', 'day');
                [u, ~, idx] = unique(d);
                c = accumarray(idx, 1);
                bar(ax2, u, c, 'r');
                grid(ax2, 'on');
            else
                text(ax2, 0.5, 0.5, 'No Anomalies Found', 'HorizontalAlignment', 'center');
            end
        catch err
            uialert(fig, err.message, 'Fetch Error');
        end
    end

    % Export plots
    function exportPlots()
        [file, path] = uiputfile({'*.pdf'; '*.png'; '*.csv'; '*.mat'}, 'Export Data As');
        if isequal(file, 0)
            return;
        end
        [~, ~, ext] = fileparts(file);
        switch lower(ext)
            case '.pdf'
                exportgraphics(ax1, fullfile(path, 'similarity_plot.pdf'));
                exportgraphics(ax2, fullfile(path, 'anomaly_plot.pdf'));
            case '.png'
                exportgraphics(ax1, fullfile(path, 'similarity_plot.png'));
                exportgraphics(ax2, fullfile(path, 'anomaly_plot.png'));
            case '.csv'
                if isempty(dataTable)
                    uialert(fig, 'No data to export!', 'Export Error');
                    return;
                end
                writetable(dataTable, fullfile(path, 'vibration_data.csv'));
            case '.mat'
                if isempty(dataTable)
                    uialert(fig, 'No data to export!', 'Export Error');
                    return;
                end
                save(fullfile(path, 'vibration_data.mat'), 'dataTable');
            otherwise
                uialert(fig, 'Unsupported format.', 'Export Error');
        end
    end

    % Initial plot load
    updatePlots();
end
